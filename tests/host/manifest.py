#!/usr/bin/env python3
"""fixtures for the provenance manifest (tests/manifest.py, docs/TESTING.md section 3): the rules a result's identity
rests on, checked without a Wine or a build.

an artifact identity is derived from the source and the artifacts, so two runs of one artifact name one identity and a
changed file names another -- including a file the revision does not hold, whose content, not only its name, is part
of the source. a selected module and a loaded module are two records: the loader's file must be the build's, and an
acceptance run has no result without that evidence. each attempt gets its own directory."""

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import manifest  # noqa: E402


def run_module(path):
    spec = importlib.util.spec_from_file_location('run', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def repository(root):
    root.mkdir(parents=True, exist_ok=True)
    subprocess.run(['git', 'init', '-q', str(root)], check=True)
    subprocess.run(['git', '-C', str(root), 'config', 'user.email', 'test@example.com'], check=True)
    subprocess.run(['git', '-C', str(root), 'config', 'user.name', 'test'], check=True)
    (root / 'src').mkdir()
    (root / 'src' / 'tracked.cpp').write_text('int a;\n')
    subprocess.run(['git', '-C', str(root), 'add', '.'], check=True)
    subprocess.run(['git', '-C', str(root), 'commit', '-qm', 'one'], check=True)
    return root


class SourceIdentity(unittest.TestCase):
    def test_identical_trees_have_one_identity(self):
        with tempfile.TemporaryDirectory() as temporary:
            at = repository(Path(temporary) / 'repo')
            first, second = manifest.source(at), manifest.source(at)
            self.assertEqual(first, second)
            self.assertEqual(manifest.digest({'source': first}), manifest.digest({'source': second}))

    def test_untracked_content_is_hashed(self):
        with tempfile.TemporaryDirectory() as temporary:
            at = repository(Path(temporary) / 'repo')
            (at / 'src' / 'new.cpp').write_text('int b;\n')
            first = manifest.source(at)
            self.assertEqual(first['added'], {'src/new.cpp': manifest.sha256(at / 'src' / 'new.cpp')})
            # editing the untracked file, without renaming it, is a different source
            (at / 'src' / 'new.cpp').write_text('int c;\n')
            second = manifest.source(at)
            self.assertNotEqual(first['added'], second['added'])
            self.assertNotEqual(manifest.digest({'source': first}), manifest.digest({'source': second}))

    def test_tracked_change_is_hashed(self):
        with tempfile.TemporaryDirectory() as temporary:
            at = repository(Path(temporary) / 'repo')
            before = manifest.source(at)
            (at / 'src' / 'tracked.cpp').write_text('int d;\n')
            after = manifest.source(at)
            self.assertNotEqual(before['changed'], after['changed'])
            self.assertEqual(after['revision'], before['revision'])


class ArtifactIdentity(unittest.TestCase):
    def said(self, loaded_name='d3d9.dll', loaded_hash='aa'):
        return {
            'source': {'revision': 'r', 'added': {}, 'changed': '', 'submodules': {}},
            'modules': {'d3d9.dll': {'sha256': 'aa', 'machine': 'x86_64', 'path': '/b/d3d9.dll', 'builtin': True}},
            'installed': {'/p/d3d9.dll': {'name': 'd3d9.dll', 'path': '/p/d3d9.dll', 'sha256': 'aa', 'built': 'aa',
                                          'machine': 'x86_64'}},
            'loaded': {loaded_name: {'sha256': loaded_hash, 'machine': 'x86_64', 'path': '/p/d3d9.dll'}},
            'programs': {},
        }

    def test_one_artifact_one_identity(self):
        self.assertEqual(manifest.digest(self.said()), manifest.digest(self.said()))

    def test_a_swapped_loaded_file_changes_the_identity(self):
        self.assertNotEqual(manifest.digest(self.said()), manifest.digest(self.said(loaded_hash='bb')))

    def test_a_swapped_prefix_bridge_changes_the_identity(self):
        first = self.said()
        first['installed']['/p/winemetal.dll'] = {'name': 'winemetal.dll', 'path': '/p/winemetal.dll', 'sha256': 'cc',
                                                  'built': 'cc', 'machine': 'x86_64'}
        second = json.loads(json.dumps(first))
        second['installed']['/p/winemetal.dll']['sha256'] = 'dd'
        self.assertNotEqual(manifest.digest(first), manifest.digest(second))


class Acceptance(unittest.TestCase):
    def said(self):
        return {
            'source': {'revision': 'r'},
            'modules': {'d3d9.dll': {'sha256': 'aa', 'machine': 'x86_64', 'path': '/b/d3d9.dll', 'builtin': True}},
            'installed': {},
            'loaded': {'d3d9.dll': {'sha256': 'aa', 'machine': 'x86_64', 'path': '/p/d3d9.dll'}},
            'programs': {'d3d9_device': {'sha256': 'ee', 'machine': 'x86_64'}},
        }

    def test_complete_record_has_no_complaint(self):
        self.assertEqual(manifest.verify(self.said()), [])

    def test_missing_source_revision_is_a_complaint(self):
        said = self.said()
        said['source'] = {'revision': None}
        self.assertTrue(manifest.verify(said))

    def test_missing_loader_evidence_is_a_complaint(self):
        said = self.said()
        said['loaded'] = {}
        self.assertTrue(manifest.verify(said))

    def test_a_prefix_file_that_is_not_the_builds_is_a_complaint(self):
        said = self.said()
        said['loaded']['d3d9.dll']['sha256'] = 'ff'
        self.assertTrue(manifest.verify(said))

    def test_a_mismatched_installation_is_a_complaint(self):
        said = self.said()
        said['installed'] = {'/p/d3d9.dll': {'name': 'd3d9.dll', 'path': '/p/d3d9.dll', 'sha256': 'aa',
                                             'built': 'ff', 'machine': 'x86_64'}}
        self.assertTrue(manifest.verify(said))

    def test_wrong_program_architecture_is_a_complaint(self):
        said = self.said()
        said['programs']['d3d9_device']['machine'] = 'i386'
        self.assertTrue(manifest.verify(said))

    def test_a_module_no_program_of_its_machine_needs_no_loader_evidence(self):
        # a PE32 run installs the second build's x86_64 libraries for the WoW64 Wine it uses, and its i386 programs
        # can never load them: their absence from the loader's record is not a gap in the evidence
        said = self.said()
        said['programs']['d3d9_device']['machine'] = 'i386'
        said['modules']['d3d9.dll']['machine'] = 'i386'
        said['modules']['nvapi64.dll'] = {'sha256': 'bb', 'machine': 'x86_64', 'path': '/b/nvapi64.dll'}
        self.assertEqual(manifest.verify(said), [])

    def test_a_module_a_program_of_its_machine_needs_loader_evidence(self):
        said = self.said()
        said['programs']['nvngx_test'] = {'sha256': 'ff', 'machine': 'x86_64'}
        said['modules']['nvapi64.dll'] = {'sha256': 'bb', 'machine': 'x86_64', 'path': '/b/nvapi64.dll'}
        self.assertTrue(manifest.verify(said))

    def test_required_manifest_fails_a_run_that_cannot_prove_itself(self):
        with tempfile.TemporaryDirectory() as temporary:
            at = Path(temporary)
            with self.assertRaises(SystemExit):
                manifest.emit([at], None, at, {}, {}, {}, tests=[], installed={}, loaded_={},
                              where=at / 'manifest.json', require=True)
            self.assertTrue((at / 'manifest.json').exists())


class Attempts(unittest.TestCase):
    def test_attempts_are_separate_and_identity_is_stable(self):
        run = run_module(HERE.parent / 'run.py')
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary)
            first = run.attempt_directory(build, 'abcdef0123456789')
            second = run.attempt_directory(build, 'abcdef0123456789')
            self.assertNotEqual(first, second)
            self.assertTrue(first.is_dir() and second.is_dir())
            self.assertEqual(first.parent, second.parent)
            # two different artifacts go to different identities
            third = run.attempt_directory(build, 'ffffffffffffffff')
            self.assertNotEqual(first.parent, third.parent)


if __name__ == '__main__':
    result = unittest.main(exit=False).result
    # the runner reads the last result line (tests/run.py::run): unittest's own 'OK' is not one
    if result.wasSuccessful():
        print(f'passed: {result.testsRun} provenance checks')
    else:
        print(f'failed: {len(result.failures) + len(result.errors)} of {result.testsRun} provenance checks')
        sys.exit(1)
