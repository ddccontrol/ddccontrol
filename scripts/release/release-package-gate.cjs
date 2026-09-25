// Attach package validation to the Release Please PR even when GITHUB_TOKEN
// suppresses pull_request events. Publication consumes that exact successful run.
const checkName = 'Release Please package build';
const workflow = '.github/workflows/release-please.yml';
const artifactNames = [
  'release-sources',
  ...require('./dbgen-assets.cjs').artifactNames,
  ...['amd64', 'arm64', 'armhf', 'ppc64el', 'riscv64', 's390x'].map(a => `packages-debian-${a}`),
  ...['x86_64', 'aarch64'].map(a => `packages-fedora-${a}`),
];

function isReleasePR(pr, repo) {
  return pr.user.login === 'github-actions[bot]' &&
    pr.head.repo?.full_name === `${repo.owner}/${repo.repo}` &&
    pr.base.ref === 'master' && pr.head.ref.startsWith('release-please--') &&
    pr.labels.some(label => label.name === 'autorelease: pending');
}

async function approvedBuild({github, repo, pr}) {
  const checks = await github.paginate(github.rest.checks.listForRef, {
    ...repo, ref: pr.head.sha, check_name: checkName, per_page: 100,
  });
  const check = checks.filter(c => c.name === checkName && c.app?.slug === 'github-actions')
    .sort((a, b) => b.id - a.id)[0];
  if (!check || check.head_sha !== pr.head.sha || check.status !== 'completed' ||
      check.conclusion !== 'success' || !/^package-run:\d+$/.test(check.external_id || '')) {
    throw new Error(`Release PR #${pr.number} has no successful current package build`);
  }
  const runId = Number(check.external_id.split(':')[1]);
  const {data: run} = await github.rest.actions.getWorkflowRun({...repo, run_id: runId});
  if (run.path !== workflow || run.status !== 'completed' || run.conclusion !== 'success') {
    throw new Error('The Release Please validation workflow must finish successfully before releasing');
  }
  const jobs = await github.paginate(github.rest.actions.listJobsForWorkflowRun, {
    ...repo, run_id: runId, filter: 'latest', per_page: 100,
  });
  if (!jobs.some(job => job.name === 'release-package-check' && job.conclusion === 'success')) {
    throw new Error('Package validation did not complete in the approved workflow run');
  }
  const {data: head} = await github.rest.repos.getCommit({...repo, ref: pr.head.sha});
  const {data: merged} = await github.rest.repos.getCommit({...repo, ref: pr.merge_commit_sha});
  if (head.commit.tree.sha !== merged.commit.tree.sha) {
    throw new Error('The merged release differs from the tested PR; update and validate the release PR before merging');
  }
  const artifacts = await github.paginate(github.rest.actions.listWorkflowRunArtifacts, {
    ...repo, run_id: runId, per_page: 100,
  });
  for (const name of artifactNames) {
    if (!artifacts.some(artifact => artifact.name === name && !artifact.expired)) {
      throw new Error(`Approved package build is missing artifact: ${name}`);
    }
  }
  return runId;
}

async function plan({github, context}) {
  const repo = context.repo;
  let candidates;
  if (context.eventName === 'pull_request') {
    // Re-fetch to reject a stale event after the PR has been updated or closed.
    const {data: pr} = await github.rest.pulls.get({...repo, pull_number: context.payload.pull_request.number});
    candidates = pr.head.sha === context.payload.pull_request.head.sha && pr.state === 'open' ? [pr] : [];
  } else {
    const issues = await github.paginate(github.rest.issues.listForRepo, {
      ...repo, labels: 'autorelease: pending', state: 'all', per_page: 100,
    });
    candidates = [];
    for (const issue of issues.filter(issue => issue.pull_request)) {
      const {data: pr} = await github.rest.pulls.get({...repo, pull_number: issue.number});
      if (pr.state === 'open' || pr.merged_at) candidates.push(pr);
    }
  }
  candidates = candidates.filter(pr => isReleasePR(pr, repo));
  // A merged pending release must be dealt with before preparing another one.
  const merged = candidates.filter(pr => pr.merged_at);
  if (merged.length) candidates = merged;
  if (!candidates.length) return {build: false, publish: false};
  if (candidates.length !== 1) throw new Error('Expected exactly one pending Release Please PR');
  const pr = candidates[0];
  const {data: manifest} = await github.rest.repos.getContent({
    ...repo, path: '.github/.release-please-manifest.json', ref: pr.head.sha,
  });
  const version = JSON.parse(Buffer.from(manifest.content, 'base64').toString())['.'];
  if (!/^\d+\.\d+\.\d+$/.test(version)) throw new Error('Expected a stable release version');
  const [major, minor] = version.split('.').map(Number);
  if (major < 3 || (major === 3 && minor < 3)) throw new Error('Package recipes require release 3.3.0 or later');
  const outputs = {commit: pr.head.sha, version, pr: pr.number};
  if (pr.merged_at) {
    const runId = await approvedBuild({github, repo, pr});
    return {...outputs, build: false, publish: true, run_id: runId};
  }
  const {data: check} = await github.rest.checks.create({
    ...repo, name: checkName, head_sha: pr.head.sha, status: 'in_progress',
    external_id: `package-run:${context.runId}`,
    details_url: `https://github.com/${repo.owner}/${repo.repo}/actions/runs/${context.runId}`,
    output: {title: 'Building release packages', summary: 'Validating all Debian and Fedora architectures before release.'},
  });
  return {...outputs, build: true, publish: false, check_id: check.id};
}

module.exports = {plan, approvedBuild, isReleasePR, checkName, artifactNames};
