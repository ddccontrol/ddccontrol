const assert = require('node:assert/strict');
const {createHash} = require('node:crypto');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const test = require('node:test');
const syncAssets = require('./release-assets.cjs');

const source = 'ddccontrol-3.3.0.tar.bz2';
const vendor = 'ddccontrol-3.3.0-vendor.tar.gz';
const bundle = 'ddccontrol-3.3.0-packages.tar.gz';
const original = Buffer.from([0, 255, 128, 42]);

async function fixture(t, files = {}) {
  const directory = await fs.mkdtemp(path.join(os.tmpdir(), 'release-assets-'));
  t.after(() => fs.rm(directory, {recursive: true, force: true}));
  const context = {repo: {owner: 'ddccontrol', repo: 'ddccontrol'}};
  const release = {id: 123, draft: false, prerelease: false};
  const assets = [];
  const contents = new Map();
  const uploads = [];
  const addAsset = (name, data) => {
    const asset = {
      id: assets.length + 1, name, state: 'uploaded', size: data.length,
      digest: `sha256:${createHash('sha256').update(data).digest('hex')}`,
    };
    assets.push(asset);
    contents.set(asset.id, data);
  };
  for (const [name, data] of Object.entries(files)) addAsset(name, data);
  const repos = new Proxy({
    getReleaseByTag: async params => {
      assert.deepEqual(params, {...context.repo, tag: '3.3.0'});
      return {data: release};
    },
    listReleaseAssets: () => assert.fail('Use pagination to list assets'),
    getReleaseAsset: async params => {
      assert.deepEqual(params, {
        ...context.repo, asset_id: params.asset_id,
        headers: {accept: 'application/octet-stream'},
      });
      // Octokit returns binary downloads as ArrayBuffer, not UTF-8 strings.
      return {data: Uint8Array.from(contents.get(params.asset_id)).buffer};
    },
    uploadReleaseAsset: async params => {
      const {name, data} = params;
      assert.deepEqual(params, {
        ...context.repo, release_id: release.id, name, data,
        headers: {'content-type': 'application/octet-stream', 'content-length': data.length},
      });
      assert.equal(assets.some(asset => asset.name === name), false, 'Never replace an asset');
      uploads.push(name);
      addAsset(name, data);
      return {data: assets.at(-1)};
    },
  }, {
    get(target, method) {
      // The failing action PATCHed release metadata before uploading anything.
      // Also reject release creation/deletion and asset deletion in these tests.
      assert.ok(method in target, `Forbidden API operation: ${String(method)}`);
      return target[method];
    },
  });
  const github = {
    rest: {repos},
    paginate: async (method, params) => {
      assert.equal(method, repos.listReleaseAssets);
      assert.deepEqual(params, {...context.repo, release_id: release.id, per_page: 100});
      return [...assets];
    },
  };
  return {github, context, tag: '3.3.0', directory, names: [source, vendor], assets, uploads};
}

test('reuse both published source archives without any write API calls', async t => {
  const options = await fixture(t, {[source]: original, [vendor]: original});
  assert.equal(await syncAssets({...options, downloadOnly: true}), true);
  for (const name of options.names) {
    assert.deepEqual(await fs.readFile(path.join(options.directory, name)), original);
  }
  assert.deepEqual(options.uploads, []);
});

test('partial publication uploads only the missing archive and restores original bytes', async t => {
  const options = await fixture(t, {[source]: original});
  assert.equal(await syncAssets({...options, downloadOnly: true}), false);
  assert.deepEqual(options.uploads, []);
  const rebuilt = Buffer.from('newly built archives');
  for (const name of options.names) await fs.writeFile(path.join(options.directory, name), rebuilt);
  assert.equal(await syncAssets(options), true);
  assert.deepEqual(options.uploads, [vendor]);
  assert.deepEqual(await fs.readFile(path.join(options.directory, source)), original);
  assert.deepEqual(await fs.readFile(path.join(options.directory, vendor)), rebuilt);
  assert.equal(await syncAssets(options), true);
  assert.deepEqual(options.uploads, [vendor], 'Retries must not upload again');
});

test('upload a new package bundle without updating the release', async t => {
  const options = await fixture(t);
  await fs.writeFile(path.join(options.directory, bundle), original);
  assert.equal(await syncAssets({...options, names: [bundle]}), true);
  assert.deepEqual(options.uploads, [bundle]);
});

test('reuse older release assets that have no digest', async t => {
  const options = await fixture(t, {[source]: original});
  options.assets[0].digest = null;
  assert.equal(await syncAssets({...options, names: [source], downloadOnly: true}), true);
  assert.deepEqual(await fs.readFile(path.join(options.directory, source)), original);
});

test('reject incomplete or corrupt published assets before packaging', async t => {
  for (const patch of [{state: 'starter'}, {size: 999}, {digest: `sha256:${'0'.repeat(64)}`}]) {
    await t.test(JSON.stringify(patch), async t => {
      const options = await fixture(t, {[source]: original});
      Object.assign(options.assets[0], patch);
      await assert.rejects(syncAssets({...options, downloadOnly: true}), /incomplete|integrity checks/);
      await assert.rejects(fs.access(path.join(options.directory, source)), {code: 'ENOENT'});
      assert.deepEqual(options.uploads, []);
    });
  }
});

test('do not create a release when the requested tag has no published release', async t => {
  const options = await fixture(t);
  options.github.rest.repos.getReleaseByTag = async () => {
    throw Object.assign(new Error('Not Found'), {status: 404});
  };
  await assert.rejects(syncAssets(options), {status: 404});
  assert.deepEqual(options.uploads, []);
});

test('propagate asset upload failures without attempting broader permissions or API writes', async t => {
  const options = await fixture(t);
  await fs.writeFile(path.join(options.directory, bundle), original);
  options.github.rest.repos.uploadReleaseAsset = async () => {
    throw Object.assign(new Error('Resource not accessible by integration'), {status: 403});
  };
  await assert.rejects(syncAssets({...options, names: [bundle]}), {status: 403});
});
