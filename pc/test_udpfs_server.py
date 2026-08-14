import os
import struct
import sys
import tempfile
import unittest
from unittest import mock


sys.path.insert(0, os.path.dirname(__file__))
import udpfs_server


class _FakeCompressedFile:
    uncompressed_size = 4096

    def close(self):
        pass


class CompressedAliasTests(unittest.TestCase):
    def _server(self, root_dir):
        server = object.__new__(udpfs_server.UdpfsServer)
        server.root_dir = os.path.realpath(root_dir)
        server.enable_compression = True
        server.compression_cache_size = 32
        server.read_only = False
        server.verbose = False
        server.handles = {}
        server.next_handle = 1
        server.stats = {'open': 0, 'dread': 0, 'getstat': 0}
        server._status_visible = False
        server._print_event = mock.Mock()
        return server

    def _all_formats_available(self):
        return mock.patch.multiple(
            udpfs_server,
            LZ4_AVAILABLE=True,
            LIBCHDR_AVAILABLE=True,
        )

    def test_virtual_names_append_iso_and_preserve_the_real_extension(self):
        server = self._server(os.curdir)
        with self._all_formats_available():
            self.assertEqual(server._transform_compressed_name('game.zso'), 'game.zso.iso')
            self.assertEqual(server._transform_compressed_name('game.cso'), 'game.cso.iso')
            self.assertEqual(server._transform_compressed_name('game.chd'), 'game.chd.iso')
            self.assertEqual(server._transform_compressed_name('game.iso'), 'game.iso')

    def test_alias_resolves_the_exact_backing_file_without_format_guessing(self):
        with tempfile.TemporaryDirectory() as root, self._all_formats_available():
            paths = {}
            for extension in ('.zso', '.cso', '.chd'):
                path = os.path.join(root, 'game' + extension)
                open(path, 'wb').close()
                paths[extension] = path

            server = self._server(root)
            for extension, path in paths.items():
                self.assertEqual(server._find_compressed_version(path + '.iso'), path)

            # The old game.iso alias guessed among several formats. Matching
            # udpfsd, aliases now retain the real extension and are unambiguous.
            self.assertIsNone(server._find_compressed_version(os.path.join(root, 'game.iso')))

    def test_unavailable_formats_are_not_advertised(self):
        server = self._server(os.curdir)
        with mock.patch.multiple(
            udpfs_server,
            LZ4_AVAILABLE=False,
            LIBCHDR_AVAILABLE=False,
        ):
            self.assertEqual(server._transform_compressed_name('game.cso'), 'game.cso.iso')
            self.assertEqual(server._transform_compressed_name('game.zso'), 'game.zso')
            self.assertEqual(server._transform_compressed_name('game.chd'), 'game.chd')

    def test_open_uses_alias_for_decompression_but_direct_name_stays_raw(self):
        with tempfile.TemporaryDirectory() as root, self._all_formats_available():
            backing_path = os.path.join(root, 'game.chd')
            with open(backing_path, 'wb') as backing:
                backing.write(b'raw chd bytes')

            server = self._server(root)
            server._send_open_reply = mock.Mock()
            compressed_stat = {'size': 4096, 'hisize': 0}

            virtual_payload = (
                struct.pack('<BBHi', 0, 0, 0, 0)
                + b'game.chd.iso\0'
            )
            with mock.patch.object(udpfs_server, 'open_compressed', return_value=_FakeCompressedFile()) as opener:
                with mock.patch.object(server, '_get_compressed_stat', return_value=compressed_stat):
                    server._handle_open(('127.0.0.1', 1), virtual_payload)
            opener.assert_called_once_with(backing_path, 32)
            server._send_open_reply.assert_called_once_with(
                ('127.0.0.1', 1), 1, stat_info=compressed_stat
            )

            server._free_handle(1)
            server._send_open_reply.reset_mock()
            direct_payload = struct.pack('<BBHi', 0, 0, 0, 0) + b'game.chd\0'
            with mock.patch.object(udpfs_server, 'open_compressed') as opener:
                server._handle_open(('127.0.0.1', 1), direct_payload)
            opener.assert_not_called()
            server._send_open_reply.assert_called_once()
            server._free_handle(2)

    def test_dread_and_getstat_use_the_same_virtual_name(self):
        with tempfile.TemporaryDirectory() as root, self._all_formats_available():
            backing_path = os.path.join(root, 'game.chd')
            with open(backing_path, 'wb') as backing:
                backing.write(b'raw chd bytes')

            server = self._server(root)
            compressed_stat = {'size': 4096, 'hisize': 0}
            server._get_compressed_stat = mock.Mock(return_value=compressed_stat)

            entries = list(os.scandir(root))
            server.handles[7] = udpfs_server.FileHandle(
                {'entries': entries, 'index': 0}, is_dir=True
            )
            server._send_dread_reply = mock.Mock()
            server._handle_dread(
                ('127.0.0.1', 1), struct.pack('<BBBBi', 0, 0, 0, 0, 7)
            )
            server._send_dread_reply.assert_called_once_with(
                ('127.0.0.1', 1),
                result=1,
                name='game.chd.iso',
                stat_info=compressed_stat,
            )

            server._send_getstat_reply = mock.Mock()
            payload = b'\0\0\0\0game.chd.iso\0'
            server._handle_getstat(('127.0.0.1', 1), payload)
            server._send_getstat_reply.assert_called_once_with(
                ('127.0.0.1', 1), result=0, stat_info=compressed_stat
            )


if __name__ == '__main__':
    unittest.main()
