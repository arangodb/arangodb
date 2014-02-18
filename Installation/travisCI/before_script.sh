d='UnitTests/HttpInterface'

echo
echo "$0: switching into ${d}"
cd "${d}" || exit 1

echo
echo "$0: installing bundler"
gem install bundler || exit 1

echo
echo "$0: executing bundle"
bundle || exit 1
