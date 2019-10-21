# boost-no-inspect

if [[ -d "$BOOST_ROOT" ]]
then
    echo Using BOOST_ROOT=$BOOST_ROOT
else
    export BOOST_ROOT=$HOME/dev/boost
fi

$BOOST_ROOT/bjam --toolset=gcc --enable-index

rm -rf convert_reference.xml
rm -rf bin




