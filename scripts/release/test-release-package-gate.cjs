const assert = require('node:assert/strict');
const test = require('node:test');
const {plan, artifactNames, checkName} = require('./release-package-gate.cjs');

function fixture() {
  const pr = {
    number: 123, state: 'open', merged_at: null, merge_commit_sha: 'b'.repeat(40),
    user: {login: 'github-actions[bot]'}, base: {ref: 'master'},
    head: {sha: 'a'.repeat(40), ref: 'release-please--branches--master', repo: {full_name: 'ddccontrol/ddccontrol'}},
    labels: [{name: 'autorelease: pending'}],
  };
  const context = {repo: {owner: 'ddccontrol', repo: 'ddccontrol'}, eventName: 'push', runId: 90};
  const data = {
    issues: [{number: pr.number, pull_request: {}}], pr, version: '3.4.0', created: [],
    checks: [{id: 10, name: checkName, head_sha: pr.head.sha, app: {slug: 'github-actions'},
      status: 'completed', conclusion: 'success', external_id: 'package-run:77'}],
    run: {path: '.github/workflows/release-please.yml', status: 'completed', conclusion: 'success'},
    jobs: [{name: 'release-package-check', conclusion: 'success'}],
    artifacts: artifactNames.map(name => ({name, expired: false})),
    trees: {[pr.head.sha]: 'same-tree', [pr.merge_commit_sha]: 'same-tree'},
  };
  const github = {
    rest: {
      issues: {listForRepo: () => data.issues},
      pulls: {get: async () => ({data: pr})},
      repos: {
        getContent: async params => {
          assert.equal(params.ref, pr.head.sha);
          return {data: {content: Buffer.from(JSON.stringify({'.': data.version})).toString('base64')}};
        },
        getCommit: async params => ({data: {commit: {tree: {sha: data.trees[params.ref]}}}}),
      },
      checks: {
        listForRef: params => {
          assert.equal(params.ref, pr.head.sha);
          assert.equal(params.check_name, checkName);
          return data.checks;
        },
        create: async params => {data.created.push(params); return {data: {id: 500}};},
      },
      actions: {
        getWorkflowRun: async params => {
          assert.equal(params.run_id, 77);
          return {data: data.run};
        },
        listJobsForWorkflowRun: params => {
          assert.equal(params.filter, 'latest');
          return data.jobs;
        },
        listWorkflowRunArtifacts: () => data.artifacts,
      },
    },
    paginate: async (method, params) => method(params),
  };
  return {github, context, data};
}

function mergedFixture() {
  const f = fixture();
  f.data.pr.state = 'closed';
  f.data.pr.merged_at = '2026-09-22T00:00:00Z';
  return f;
}

test('ordinary activity with no release PR neither builds nor publishes', async () => {
  const f = fixture();
  f.data.issues = [];
  assert.deepEqual(await plan(f), {build: false, publish: false});
  assert.deepEqual(f.data.created, []);
});

test('an open bot release PR gets a check on its head and starts validation, never publication', async () => {
  const f = fixture();
  const result = await plan(f);
  assert.deepEqual(result, {
    commit: f.data.pr.head.sha, version: '3.4.0', pr: 123,
    build: true, publish: false, check_id: 500,
  });
  assert.equal(f.data.created[0].head_sha, f.data.pr.head.sha);
  assert.equal(f.data.created[0].status, 'in_progress');
  assert.equal(f.data.created[0].external_id, 'package-run:90');
});

test('normal, forked, unlabelled and non-release PRs cannot start privileged builds', async t => {
  for (const kind of ['author', 'fork', 'label', 'branch', 'base']) {
    await t.test(kind, async () => {
      const f = fixture();
      if (kind === 'author') f.data.pr.user.login = 'contributor';
      if (kind === 'fork') f.data.pr.head.repo.full_name = 'someone/ddccontrol';
      if (kind === 'label') f.data.pr.labels = [];
      if (kind === 'branch') f.data.pr.head.ref = 'feature';
      if (kind === 'base') f.data.pr.base.ref = 'other';
      assert.deepEqual(await plan(f), {build: false, publish: false});
      assert.deepEqual(f.data.created, []);
    });
  }
});

test('a stale pull_request event cannot validate a newer PR revision', async () => {
  const f = fixture();
  f.context.eventName = 'pull_request';
  f.context.payload = {pull_request: {number: 123, head: {sha: 'old'}}};
  assert.deepEqual(await plan(f), {build: false, publish: false});
});

test('a current release PR event starts validation directly', async () => {
  const f = fixture();
  f.context.eventName = 'pull_request';
  f.context.payload = {pull_request: {number: 123, head: {sha: f.data.pr.head.sha}}};
  assert.equal((await plan(f)).build, true);
});

test('a merged release reuses the approved artifacts without another package build', async () => {
  const f = mergedFixture();
  assert.deepEqual(await plan(f), {
    commit: f.data.pr.head.sha, version: '3.4.0', pr: 123,
    build: false, publish: true, run_id: 77,
  });
  assert.deepEqual(f.data.created, []);
});

test('missing, failed, skipped, cancelled and stale checks prevent release', async t => {
  for (const kind of ['missing', 'failure', 'skipped', 'cancelled', 'running', 'stale', 'untrusted', 'invalid-run']) {
    await t.test(kind, async () => {
      const f = mergedFixture();
      const check = f.data.checks[0];
      if (kind === 'missing') f.data.checks = [];
      else if (kind === 'running') check.status = 'in_progress';
      else if (kind === 'stale') check.head_sha = 'old-head';
      else if (kind === 'untrusted') check.app.slug = 'another-app';
      else if (kind === 'invalid-run') check.external_id = 'invalid';
      else check.conclusion = kind;
      await assert.rejects(plan(f), /no successful current package build/);
      assert.deepEqual(f.data.created, []);
    });
  }
});

test('an older green result cannot override the latest failed build', async () => {
  const f = mergedFixture();
  f.data.checks.push({...f.data.checks[0], id: 11, conclusion: 'failure'});
  await assert.rejects(plan(f), /no successful current package build/);
});

test('green checks from incomplete or unrelated workflow runs cannot authorize release', async t => {
  for (const kind of ['failure', 'cancelled', 'running', 'wrong-workflow', 'missing-job', 'skipped-job']) {
    await t.test(kind, async () => {
      const f = mergedFixture();
      if (kind === 'running') f.data.run.status = 'in_progress';
      else if (kind === 'wrong-workflow') f.data.run.path = '.github/workflows/ci.yml';
      else if (kind === 'missing-job') f.data.jobs = [];
      else if (kind === 'skipped-job') f.data.jobs[0].conclusion = 'skipped';
      else f.data.run.conclusion = kind;
      await assert.rejects(plan(f), /workflow|validation/);
    });
  }
});

test('merging untested code after the package build prevents release', async () => {
  const f = mergedFixture();
  f.data.trees[f.data.pr.merge_commit_sha] = 'different-tree';
  await assert.rejects(plan(f), /differs from the tested PR/);
});

test('every source, producer, Debian and Fedora artifact is required and must not have expired', async t => {
  for (const name of artifactNames) {
    await t.test(name, async () => {
      const f = mergedFixture();
      f.data.artifacts.find(a => a.name === name).expired = true;
      await assert.rejects(plan(f), /missing artifact/);
      f.data.artifacts = f.data.artifacts.filter(a => a.name !== name);
      await assert.rejects(plan(f), /missing artifact/);
    });
  }
});

test('a package-only validation run cannot authorize a release with standalone producers', async () => {
  const f = mergedFixture();
  f.data.artifacts = f.data.artifacts.filter(a => !a.name.startsWith('dbgen-'));
  await assert.rejects(plan(f), /missing artifact: dbgen-/);
});

test('unstable versions never start a release build', async () => {
  const f = fixture();
  f.data.version = '3.4.0-rc1';
  await assert.rejects(plan(f), /stable release/);
  assert.deepEqual(f.data.created, []);
});


test('unsupported package versions cannot be published later', async () => {
  const f = fixture();
  f.data.version = '3.2.1';
  await assert.rejects(plan(f), /3.3.0 or later/);
  assert.deepEqual(f.data.created, []);
});

test('closing an unmerged release PR does not authorize publication', async () => {
  const f = fixture();
  f.data.pr.state = 'closed';
  assert.deepEqual(await plan(f), {build: false, publish: false});
});
