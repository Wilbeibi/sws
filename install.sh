PLATFORM=`uname -s`
echo "Compiling sws on" $PLATFORM ......
cat Makefile | sed s/Make.on.\\\$\(PLATFORM\)/Make.on.$PLATFORM/g > _make
make -f _make
rm _make
