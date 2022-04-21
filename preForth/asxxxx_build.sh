#!/bin/sh

# put asxs5p30.zip in current directory first
# wget http://shop-pdp.net/_ftp/asxxxx/asxs5p30.zip

rm -rf asxxxx_build
mkdir asxxxx_build
(cd asxxxx_build && unzip -L -a ../asxs5p30)
(cd asxxxx_build/asxv5pxx/asxmak/linux/build && make asz80 aslink)
mkdir --parents bin
cp asxxxx_build/asxv5pxx/asxmak/linux/exe/as* bin
