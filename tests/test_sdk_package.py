"""Publication checks without compiling the renderer: python3 tests/test_sdk_package.py."""
import hashlib
import importlib.util
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('ymgre_package', Path(__file__).resolve().parents[1] / 'sdk/package.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class PackagePublicationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='ymgre-publish-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.out = self.root / 'build/YMGRE_libs'
        self.out.mkdir(parents=True)
        (self.out / '.ymgre-sdk').write_text('managed')
        (self.out / 'old.txt').write_text('previous SDK')
        self.releases = self.root / 'releases'
        self.releases.mkdir()
        self.name = f'YMGRE_libs-{package.platform.system().lower()}-{package.platform.machine()}.tar.gz'
        self.archive = self.releases / self.name
        self.archive.write_bytes(b'previous archive')
        self.checksum = self.releases / (self.name + '.sha256')
        self.checksum.write_bytes(b'previous checksum')
        self.patch = patch.multiple(package, ROOT=self.root, OUT=self.out, WORK=self.root / 'build/_ymgre_sdk')
        self.patch.start()
        self.addCleanup(self.patch.stop)

    @staticmethod
    def build_fixture(stage, args):
        (stage / 'new.txt').write_text('verified SDK fixture')

    def invoke(self, *args, failure=None):
        build = failure if failure else self.build_fixture
        with patch.object(package, 'build_package', side_effect=build), patch.object(sys, 'argv', ['package.py', *args]):
            package.main()

    def assert_old_release(self):
        self.assertEqual(self.archive.read_bytes(), b'previous archive')
        self.assertEqual(self.checksum.read_bytes(), b'previous checksum')

    def test_local_build_does_not_publish_release(self):
        self.invoke()
        self.assert_old_release()
        self.assertTrue((self.out.parent / self.name).is_file())
        self.assertTrue((self.out / 'new.txt').is_file())

    def test_release_has_valid_archive_root_and_checksum(self):
        self.invoke('--release')
        expected = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        self.assertEqual(self.checksum.read_text(), expected + '  ' + self.name + '\n')
        with tarfile.open(self.archive) as bundle:
            self.assertIn('YMGRE_libs/new.txt', bundle.getnames())
            self.assertNotIn('YMGRE_libs/old.txt', bundle.getnames())
        self.assertFalse((self.out.parent / self.name).exists())

    def test_failed_validation_preserves_previous_sdk_and_release(self):
        def fail(stage, args):
            (stage / 'partial.txt').write_text('unfinished')
            raise RuntimeError('injected validation failure')
        with self.assertRaisesRegex(RuntimeError, 'validation failure'):
            self.invoke('--release', failure=fail)
        self.assertEqual((self.out / 'old.txt').read_text(), 'previous SDK')
        self.assertFalse((self.out / 'partial.txt').exists())
        self.assert_old_release()
        self.assertEqual(list((self.root / 'build/_ymgre_sdk').iterdir()), [])

    def test_compression_failure_does_not_truncate_release(self):
        with patch.object(package.tarfile, 'open', side_effect=OSError('injected compression failure')):
            with self.assertRaisesRegex(OSError, 'compression failure'):
                self.invoke('--release')
        self.assert_old_release()
        self.assertFalse(list(self.releases.glob('.ymgre-release-*')))


if __name__ == '__main__':
    unittest.main()
