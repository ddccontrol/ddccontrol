// Keep the released producer and its checksum together, including recovery after
// a partially completed upload. Already published bytes are never replaced.
const {createHash} = require('node:crypto');
const fs = require('node:fs/promises');
const path = require('node:path');
const syncAssets = require('./release-assets.cjs');

const targets = ['x86_64-unknown-linux-musl', 'aarch64-unknown-linux-musl'];
const artifactNames = targets.map(target => `dbgen-${target}`);
function assetNames(version) {
  if (!/^\d+\.\d+\.\d+$/.test(version)) throw new Error('Expected a stable release version');
  return targets.flatMap(target => {
    const archive = `ddccontrol-dbgen-${version}-${target}.tar.gz`;
    return [archive, `${archive}.sha256`];
  });
}

async function syncDbgenAssets({version, ...options}) {
  const names = assetNames(version);
  // Preflight every existing pair before making any write API calls.
  for (let i = 0; i < names.length; i += 2) {
    const [archive, checksum] = names.slice(i, i + 2);
    await syncAssets({...options, names: [archive], downloadOnly: true});
    const hasChecksum = await syncAssets({...options, names: [checksum], downloadOnly: true});
    const bytes = await fs.readFile(path.join(options.directory, archive));
    const expected = `${createHash('sha256').update(bytes).digest('hex')}  ${archive}\n`;
    const checksumPath = path.join(options.directory, checksum);
    if (hasChecksum) {
      if (await fs.readFile(checksumPath, 'utf8') !== expected) {
        throw new Error(`Published producer checksum does not match: ${archive}`);
      }
    } else {
      // The published archive may differ from a rebuild. Hash its actual bytes.
      await fs.writeFile(checksumPath, expected);
    }
  }
  return syncAssets({...options, names});
}

module.exports = {targets, artifactNames, assetNames, syncDbgenAssets};
