#! /usr/bin/env python3
import subprocess
import time
import glob
import os
import tempfile
import unittest

SIZES = [3, 11]  # Always 2 sizes
MIB = 1048576
LZ4 = os.path.dirname(os.path.realpath(__file__)) + "/../lz4"
if not os.path.exists(LZ4):
    LZ4 = os.path.dirname(os.path.realpath(__file__)) + "/../programs/lz4"
TEMP = tempfile.gettempdir()


class NVerboseFileInfo(object):
    def __init__(self, line_in):
        self.line = line_in
        splitlines = line_in.split()
        if len(splitlines) != 7:
            errout("Unexpected line: {}".format(line_in))
        self.frames, self.type, self.block, self.compressed, self.uncompressed, self.ratio, self.filename = splitlines
        self.exp_unc_size = 0
        # Get real file sizes
        if "concat-all" in self.filename or "2f--content-size" in self.filename:
            for i in SIZES:
                self.exp_unc_size += os.path.getsize("{}/test_list_{}M".format(TEMP, i))
        else:
            uncompressed_filename = self.filename.split("-")[0]
            self.exp_unc_size += os.path.getsize("{}/{}".format(TEMP, uncompressed_filename))
        self.exp_comp_size = os.path.getsize("{}/{}".format(TEMP, self.filename))


class TestNonVerbose(unittest.TestCase):
    @classmethod
    def setUpClass(self):
        self.nvinfo_list = []
        for i, line in enumerate(execute("{} --list -m {}/test_list_*.lz4".format(LZ4, TEMP), print_output=True)):
            if i > 0:
                self.nvinfo_list.append(NVerboseFileInfo(line))

    def test_frames(self):
        all_concat_frames = 0
        all_concat_index = None
        for i, nvinfo in enumerate(self.nvinfo_list):
            if "concat-all" in nvinfo.filename:
                all_concat_index = i
            elif "2f--content-size" in nvinfo.filename:
                self.assertEqual("2", nvinfo.frames, nvinfo.line)
                all_concat_frames += 2
            else:
                self.assertEqual("1", nvinfo.frames, nvinfo.line)
                all_concat_frames += 1
        self.assertNotEqual(None, all_concat_index, "Couldn't find concat-all file index.")
        self.assertEqual(self.nvinfo_list[all_concat_index].frames, str(all_concat_frames), self.nvinfo_list[all_concat_index].line)

    def test_frame_types(self):
        for nvinfo in self.nvinfo_list:
            if "-lz4f-" in nvinfo.filename:
                self.assertEqual(nvinfo.type, "LZ4Frame", nvinfo.line)
            elif "-legc-" in nvinfo.filename:
                self.assertEqual(nvinfo.type, "LegacyFrame", nvinfo.line)
            elif "-skip-" in nvinfo.filename:
                self.assertEqual(nvinfo.type, "SkippableFrame", nvinfo.line)

    def test_block(self):
        for nvinfo in self.nvinfo_list:
            # if "-leg" in nvinfo.filename or "-skip" in nvinfo.filename:
            #     self.assertEqual(nvinfo.block, "-", nvinfo.line)
            if "--BD" in nvinfo.filename:
                self.assertRegex(nvinfo.block, "^B[0-9]+D$", nvinfo.line)
            elif "--BI" in nvinfo.filename:
                self.assertRegex(nvinfo.block, "^B[0-9]+I$", nvinfo.line)

    def test_compressed_size(self):
        for nvinfo in self.nvinfo_list:
            self.assertEqual(nvinfo.compressed, to_human(nvinfo.exp_comp_size), nvinfo.line)

    def test_ratio(self):
        for nvinfo in self.nvinfo_list:
            if "--content-size" in nvinfo.filename:
                self.assertEqual(nvinfo.ratio, "{:.2f}%".format(float(nvinfo.exp_comp_size) / float(nvinfo.exp_unc_size) * 100), nvinfo.line)

    def test_uncompressed_size(self):
        for nvinfo in self.nvinfo_list:
            if "--content-size" in nvinfo.filename:
                self.assertEqual(nvinfo.uncompressed, to_human(nvinfo.exp_unc_size), nvinfo.line)


class VerboseFileInfo(object):
    def __init__(self, lines):
        # Parse lines
        self.frame_list = []
        self.file_frame_map = []
        for i, line in enumerate(lines):
            if i == 0:
                self.filename = line
                continue
            elif i == 1:
                # Skip header
                continue
            frame_info = dict(zip(["frame", "type", "block", "checksum", "compressed", "uncompressed", "ratio"], line.split()))
            frame_info["line"] = line
            self.frame_list.append(frame_info)


class TestVerbose(unittest.TestCase):
    @classmethod
    def setUpClass(self):
        # Even do we're listing 2 files to test multiline working as expected.
        # we're only really interested in testing the output of the concat-all file.
        self.vinfo_list = []
        start = end = 0
        output = execute("{} --list -m -v {}/test_list_concat-all.lz4 {}/test_list_*M-lz4f-2f--content-size.lz4".format(LZ4, TEMP, TEMP), print_output=True)
        for i, line in enumerate(output):
            if line.startswith("test_list"):
                if start != 0 and end != 0:
                    self.vinfo_list.append(VerboseFileInfo(output[start:end]))
                start = i
            if not line:
                end = i
        self.vinfo_list.append(VerboseFileInfo(output[start:end]))
        # Populate file_frame_map as a reference of the expected info
        concat_file_list = glob.glob("/tmp/test_list_[!concat]*.lz4")
        # One of the files has 2 frames so duplicate it in this list to map each frame 1 to a single file
        for i, filename in enumerate(concat_file_list):
            if "2f--content-size" in filename:
                concat_file_list.insert(i, filename)
                break
        self.cvinfo = self.vinfo_list[0]
        self.cvinfo.file_frame_map = concat_file_list
        self.cvinfo.compressed_size = os.path.getsize("{}/test_list_concat-all.lz4".format(TEMP))

    def test_filename(self):
        for i, vinfo in enumerate(self.vinfo_list):
            self.assertRegex(vinfo.filename, "^test_list_.*({}/{})".format(i + 1, len(self.vinfo_list)))

    def test_frame_number(self):
        for vinfo in self.vinfo_list:
            for i, frame_info in enumerate(vinfo.frame_list):
                self.assertEqual(frame_info["frame"], str(i + 1), frame_info["line"])

    def test_frame_type(self):
        for i, frame_info in enumerate(self.cvinfo.frame_list):
            if "-lz4f-" in self.cvinfo.file_frame_map[i]:
                self.assertEqual(self.cvinfo.frame_list[i]["type"], "LZ4Frame", self.cvinfo.frame_list[i]["line"])
            elif "-legc-" in self.cvinfo.file_frame_map[i]:
                self.assertEqual(self.cvinfo.frame_list[i]["type"], "LegacyFrame", self.cvinfo.frame_list[i]["line"])
            elif "-skip-" in self.cvinfo.file_frame_map[i]:
                self.assertEqual(self.cvinfo.frame_list[i]["type"], "SkippableFrame", self.cvinfo.frame_list[i]["line"])

    def test_block(self):
        for i, frame_info in enumerate(self.cvinfo.frame_list):
            if "--BD" in self.cvinfo.file_frame_map[i]:
                self.assertRegex(self.cvinfo.frame_list[i]["block"], "^B[0-9]+D$", self.cvinfo.frame_list[i]["line"])
            elif "--BI" in self.cvinfo.file_frame_map[i]:
                self.assertEqual(self.cvinfo.frame_list[i]["block"], "^B[0-9]+I$", self.cvinfo.frame_list[i]["line"])

    def test_checksum(self):
        for i, frame_info in enumerate(self.cvinfo.frame_list):
            if "-lz4f-" in self.cvinfo.file_frame_map[i] and "--no-frame-crc" not in self.cvinfo.file_frame_map[i]:
                self.assertEqual(self.cvinfo.frame_list[i]["checksum"], "XXH32", self.cvinfo.frame_list[i]["line"])

    def test_compressed(self):
        total = 0
        for i, frame_info in enumerate(self.cvinfo.frame_list):
            if "-2f-" not in self.cvinfo.file_frame_map[i]:
                expected_size = os.path.getsize(self.cvinfo.file_frame_map[i])
                self.assertEqual(self.cvinfo.frame_list[i]["compressed"], str(expected_size), self.cvinfo.frame_list[i]["line"])
            total += int(self.cvinfo.frame_list[i]["compressed"])
        self.assertEqual(total, self.cvinfo.compressed_size, "Expected total sum ({}) to match {} filesize".format(total, self.cvinfo.filename))

    def test_uncompressed(self):
        for i, frame_info in enumerate(self.cvinfo.frame_list):
            ffm = self.cvinfo.file_frame_map[i]
            if "-2f-" not in ffm and "--content-size" in ffm:
                expected_size_unc = int(ffm[ffm.rindex("_") + 1:ffm.index("M")]) * 1048576
                self.assertEqual(self.cvinfo.frame_list[i]["uncompressed"], str(expected_size_unc), self.cvinfo.frame_list[i]["line"])

    def test_ratio(self):
        for i, frame_info in enumerate(self.cvinfo.frame_list):
            if "--content-size" in self.cvinfo.file_frame_map[i]:
                self.assertEqual(self.cvinfo.frame_list[i]['ratio'],
                                 "{:.2f}%".format(float(self.cvinfo.frame_list[i]['compressed']) / float(self.cvinfo.frame_list[i]['uncompressed']) * 100),
                                 self.cvinfo.frame_list[i]["line"])


def to_human(size):
    for unit in ['', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y']:
        if size < 1024.0:
            break
        size /= 1024.0
    return "{:.2f}{}".format(size, unit)


def log(text):
    print(time.strftime("%Y/%m/%d %H:%M:%S") + ' - ' + text)


def errout(text, err=1):
    log(text)
    exit(err)


def execute(command, print_command=True, print_output=False, print_error=True, param_shell=True):
    if os.environ.get('QEMU_SYS'):
        command = "{} {}".format(os.environ['QEMU_SYS'], command)
    if print_command:
        log("> " + command)
    popen = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=param_shell)
    stdout_lines, stderr_lines = popen.communicate()
    stderr_lines = stderr_lines.decode("utf-8")
    stdout_lines = stdout_lines.decode("utf-8")
    if print_output:
        if stdout_lines:
            print(stdout_lines)
        if stderr_lines:
            print(stderr_lines)
    if popen.returncode is not None and popen.returncode != 0:
        if stderr_lines and not print_output and print_error:
            print(stderr_lines)
        errout("Failed to run: {}\n".format(command, stdout_lines + stderr_lines))
    return (stdout_lines + stderr_lines).splitlines()


def cleanup(silent=False):
    for f in glob.glob("{}/test_list*".format(TEMP)):
        if not silent:
            log("Deleting {}".format(f))
        os.unlink(f)


def datagen(file_name, size):
    non_sparse_size = size // 2
    sparse_size = size - non_sparse_size
    with open(file_name, "wb") as f:
        f.seek(sparse_size)
        f.write(os.urandom(non_sparse_size))


def generate_files():
    # file format  ~ test_list<frametype>-<no_frames>f<create-args>.lz4 ~
    # Generate LZ4Frames
    for i in SIZES:
        filename = "{}/test_list_{}M".format(TEMP, i)
        log("Generating {}".format(filename))
        datagen(filename, i * MIB)
        for j in ["--content-size", "-BI", "-BD", "-BX", "--no-frame-crc"]:
            lz4file = "{}-lz4f-1f{}.lz4".format(filename, j)
            execute("{} {} {} {}".format(LZ4, j, filename, lz4file))
        # Generate skippable frames
        lz4file = "{}-skip-1f.lz4".format(filename)
        skipsize = i * 1024
        skipbytes = bytes([80, 42, 77, 24]) + skipsize.to_bytes(4, byteorder='little', signed=False)
        with open(lz4file, 'wb') as f:
            f.write(skipbytes)
            f.write(os.urandom(skipsize))
        # Generate legacy frames
        lz4file = "{}-legc-1f.lz4".format(filename)
        execute("{} -l {} {}".format(LZ4, filename, lz4file))

    # Concatenate --content-size files
    file_list = glob.glob("{}/test_list_*-lz4f-1f--content-size.lz4".format(TEMP))
    with open("{}/test_list_{}M-lz4f-2f--content-size.lz4".format(TEMP, sum(SIZES)), 'ab') as outfile:
        for fname in file_list:
            with open(fname, 'rb') as infile:
                outfile.write(infile.read())

    # Concatenate all files
    file_list = glob.glob("{}/test_list_*.lz4".format(TEMP))
    with open("{}/test_list_concat-all.lz4".format(TEMP), 'ab') as outfile:
        for fname in file_list:
            with open(fname, 'rb') as infile:
                outfile.write(infile.read())


if __name__ == '__main__':
    cleanup()
    generate_files()
    unittest.main(verbosity=2, exit=False)
    cleanup(silent=True)
