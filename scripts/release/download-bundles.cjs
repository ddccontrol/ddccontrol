// Run by actions/github-script. Releases are durable storage for all versions;
// rebuilding Pages must not depend on an expiring Actions cache or artifact.
const fs = require('node:fs/promises');
const path = require('node:path');

module.exports = async ({github, context}) => {
  const directory = process.env.BUNDLE_DIRECTORY;
  await fs.mkdir(directory, {recursive: true});
  const releases = await github.paginate(github.rest.repos.listReleases, {
    ...context.repo, per_page: 100,
  });
  let count = 0;
  for (const release of releases) {
    if (release.draft || release.prerelease) continue;
    const assets = await github.paginate(github.rest.repos.listReleaseAssets, {
      ...context.repo, release_id: release.id, per_page: 100,
    });
    for (const asset of assets) {
      if (!/^ddccontrol-\d+\.\d+\.\d+-packages\.tar\.gz$/.test(asset.name)) continue;
      const response = await github.rest.repos.getReleaseAsset({
        ...context.repo, asset_id: asset.id,
        headers: {accept: 'application/octet-stream'},
      });
      await fs.writeFile(path.join(directory, asset.name), Buffer.from(response.data), {flag: 'wx'});
      count++;
    }
  }
  if (!count) throw new Error('No release package bundles found');
};
