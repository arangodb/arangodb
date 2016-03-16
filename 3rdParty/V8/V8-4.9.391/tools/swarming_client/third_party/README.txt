Packages in this directory are installed / copied from other
repositories. When vendoring a package to be compatible with
infra.git, please use glyco tool:

  https://chromium.googlesource.com/infra/infra/+/HEAD/glyco/README.md

For example, installing google-api-python-client:

Look up the revision used in
  https://chromium.googlesource.com/infra/infra/+/master/bootstrap/deps.pyl
At the time of writing, it's d83246e69b22f084d1ae92da5897572a4a4eb03d.

cd <scratch dir>
git clone https://chromium.googlesource.com/external/github.com/google/google-api-python-client
cd google-api-python-client
git checkout d83246e69b22f084d1ae92da5897572a4a4eb03d
glyco pack . -o <wheelhouse dir>

cd <luci-py checkout>/client/third_party
glyco install -i . <wheelhouse dir>/google_api_python_client-1.4.2-*

# Local modification - document it in README.swarming
rm -rf apiclient
