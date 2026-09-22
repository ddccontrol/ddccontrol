// Run by actions/github-script. Only add missing assets to a published release;
// do not PATCH its metadata just to upload files.
const {createHash} = require('node:crypto');
const fs = require('node:fs/promises');
const path = require('node:path');

module.exports = async ({github, context, tag, directory, names, downloadOnly = false}) => {
  const {data: release} = await github.rest.repos.getReleaseByTag({...context.repo, tag});
  if (release.draft || release.prerelease) throw new Error('Release must be published and stable');
  const assets = await github.paginate(github.rest.repos.listReleaseAssets, {
    ...context.repo, release_id: release.id, per_page: 100,
  });
  await fs.mkdir(directory, {recursive: true});
  let complete = true;
  for (const name of names) {
    const file = path.join(directory, name);
    const asset = assets.find(candidate => candidate.name === name);
    if (asset) {
      if (asset.state !== 'uploaded') throw new Error(`Release asset is incomplete: ${name}`);
      const response = await github.rest.repos.getReleaseAsset({
        ...context.repo, asset_id: asset.id,
        headers: {accept: 'application/octet-stream'},
      });
      const data = Buffer.from(response.data);
      const digest = `sha256:${createHash('sha256').update(data).digest('hex')}`;
      if (data.length !== asset.size || (asset.digest && asset.digest !== digest)) {
        throw new Error(`Release asset failed integrity checks: ${name}`);
      }
      // Published bytes take precedence over any locally rebuilt archive.
      await fs.writeFile(file, data);
      console.log(`Reusing release asset: ${name}`);
    } else if (downloadOnly) {
      complete = false;
    } else {
      const data = await fs.readFile(file);
      await github.rest.repos.uploadReleaseAsset({
        ...context.repo, release_id: release.id, name, data,
        headers: {'content-type': 'application/octet-stream', 'content-length': data.length},
      });
      console.log(`Uploaded release asset: ${name}`);
    }
  }
  return complete;
};
