if test x = x$BOOST_ROOT 
then
    echo BOOST_ROOT not set
    exit 1
fi
xsltproc --nonet --xinclude bb2db.xsl safe_numerics.xml | xsltproc --nonet db2html.xsl -
cp pre-boost.jpg ../html
cp pre-boost.jpg ../html/eliminate_runtime_penalty
cp pre-boost.jpg ../html/promotion_policies
cp pre-boost.jpg ../html/tutorial
cp StepperMotor.gif ../html/
cp stepper_profile.png ../html/
cp $BOOST_ROOT/doc/src/boostbook.css ../html
cp -R $BOOST_ROOT/doc/html/images ../html
