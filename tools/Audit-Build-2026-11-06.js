// Read-only game-source audit runner; writes only its own dated evidence artifact.
const fs = require('fs');
const cp = require('child_process');
const path = require('path');
const root = path.resolve(__dirname, '..');
const sourceRoot = process.argv[2] ? path.resolve(process.argv[2]) : root;
const testRoot = path.join(sourceRoot, 'tools');
const results = [];
for (const name of fs.readdirSync(testRoot).filter(n => /^Test-.*\.js$/.test(n))) {
  const result = cp.spawnSync(process.execPath, [path.join(testRoot, name)], {
    cwd: sourceRoot, encoding: 'utf8', timeout: 120000
  });
  const output = (result.stdout || '') + (result.stderr || '');
  const assertions = Number(output.match(/["']?assertions["']?\s*:\s*(\d+)/)?.[1] || 0);
  const completed = assertions > 0 || (name === 'Test-PresetVehicles.js' && output.includes('runtime availability checks passed.'));
  const passed = result.status === 0 && !result.error && completed &&
    !/(?:AssertionError|SyntaxError|ReferenceError|TypeError):/.test(output);
  results.push({ name, passed, assertions, exitCode: result.status, output, error: result.error?.message });
  console.log(`${passed ? 'PASS' : 'FAIL'} ${name} (${assertions} assertions)`);
  if (!passed) console.log(output.slice(-3500));
}
const head = cp.spawnSync('git', ['rev-parse', 'HEAD'], { cwd: root, encoding: 'utf8' }).stdout?.trim();
const reportName = process.argv[4] || (process.argv[2] ? 'BUILD_AUDIT_2026-11-06_ROWAN_TESTS.json' : 'BUILD_AUDIT_2026-11-06_TESTS.json');
fs.writeFileSync(path.join(root, 'docs/validation', reportName), JSON.stringify({
  generatedAt: new Date().toISOString(), head: process.argv[2] ? (process.argv[3] || 'upstream/master-dev (3dba0bd)') : head, runtime: process.version, nativeVerified: false, results
}, null, 2) + '\n');
console.log(JSON.stringify({ passed: results.filter(r => r.passed).length, total: results.length }));
process.exitCode = results.every(r => r.passed) ? 0 : 1;
