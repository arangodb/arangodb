# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions used to provision Fuchsia boot images."""

import common
import logging
import os
import subprocess
import tempfile
import time
import uuid

_SSH_CONFIG_TEMPLATE = """
Host *
  CheckHostIP no
  StrictHostKeyChecking no
  ForwardAgent no
  ForwardX11 no
  UserKnownHostsFile {known_hosts}
  User fuchsia
  IdentitiesOnly yes
  IdentityFile {identity}
  ServerAliveInterval 2
  ServerAliveCountMax 5
  ControlMaster auto
  ControlPersist 1m
  ControlPath /tmp/ssh-%r@%h:%p
  ConnectTimeout 5
  """

FVM_TYPE_QCOW = 'qcow'
FVM_TYPE_SPARSE = 'sparse'


def _TargetCpuToSdkBinPath(target_arch):
  """Returns the path to the SDK 'target' file directory for |target_cpu|."""

  return os.path.join(common.SDK_ROOT, 'target', target_arch)


def _GetPubKeyPath(output_dir):
  """Returns a path to the generated SSH public key."""

  return os.path.join(output_dir, 'id_ed25519.pub')


def ProvisionSSH(output_dir):
  """Generates a keypair and config file for SSH."""

  host_key_path = os.path.join(output_dir, 'ssh_key')
  host_pubkey_path = host_key_path + '.pub'
  id_key_path = os.path.join(output_dir, 'id_ed25519')
  id_pubkey_path = _GetPubKeyPath(output_dir)
  known_hosts_path = os.path.join(output_dir, 'known_hosts')
  ssh_config_path = os.path.join(output_dir, 'ssh_config')

  logging.debug('Generating SSH credentials.')
  if not os.path.isfile(host_key_path):
    subprocess.check_call(['ssh-keygen', '-t', 'ed25519', '-h', '-f',
                           host_key_path, '-P', '', '-N', ''],
                          stdout=open(os.devnull))
  if not os.path.isfile(id_key_path):
    subprocess.check_call(['ssh-keygen', '-t', 'ed25519', '-f', id_key_path,
                           '-P', '', '-N', ''], stdout=open(os.devnull))

  with open(ssh_config_path, "w") as ssh_config:
    ssh_config.write(
        _SSH_CONFIG_TEMPLATE.format(identity=id_key_path,
                                    known_hosts=known_hosts_path))

  if os.path.exists(known_hosts_path):
    os.remove(known_hosts_path)


def _MakeQcowDisk(output_dir, disk_path):
  """Creates a QEMU copy-on-write version of |disk_path| in the output
  directory."""

  qimg_path = os.path.join(common.GetQemuRootForPlatform(), 'bin', 'qemu-img')
  output_path = os.path.join(output_dir,
                             os.path.basename(disk_path) + '.qcow2')
  subprocess.check_call([qimg_path, 'create', '-q', '-f', 'qcow2',
                         '-b', disk_path, output_path])
  return output_path


def GetTargetFile(target_arch, filename):
  """Computes a path to |filename| in the Fuchsia target directory specific to
  |target_arch|."""

  return os.path.join(_TargetCpuToSdkBinPath(target_arch), filename)


def GetSSHConfigPath(output_dir):
  return output_dir + '/ssh_config'


def GetBootImage(output_dir, target_arch):
  """"Gets a path to the Zircon boot image, with the SSH client public key
  added."""

  ProvisionSSH(output_dir)
  pubkey_path = _GetPubKeyPath(output_dir)
  zbi_tool = os.path.join(common.SDK_ROOT, 'tools', 'zbi')
  image_source_path = GetTargetFile(target_arch, 'fuchsia.zbi')
  image_dest_path = os.path.join(output_dir, 'gen', 'fuchsia-with-keys.zbi')

  cmd = [ zbi_tool, '-o', image_dest_path, image_source_path,
          '-e', 'data/ssh/authorized_keys=' + pubkey_path ]
  subprocess.check_call(cmd)

  return image_dest_path


def GetNodeName(output_dir):
  """Returns the cached Zircon node name, or generates one if it doesn't
  already exist. The node name is used by Discover to find the prior
  deployment on the LAN."""

  nodename_file = os.path.join(output_dir, 'nodename')
  if not os.path.exists(nodename_file):
    nodename = uuid.uuid4()
    f = open(nodename_file, 'w')
    f.write(str(nodename))
    f.flush()
    f.close()
    return str(nodename)
  else:
    f = open(nodename_file, 'r')
    return f.readline()


def GetKernelArgs(output_dir):
  return ['devmgr.epoch=%d' % time.time(),
          'zircon.nodename=' + GetNodeName(output_dir)]
