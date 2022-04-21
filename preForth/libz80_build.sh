#!/bin/sh

# put lib80-2.1.0.tar.gz in current directory first
# wget https://sourceforge.net/settings/mirror_choices?projectname=libz80&filename=libz80/2.1/libz80-2.1.0.tar.gz&selected=master

rm -rf libz80_build
mkdir libz80_build
(cd libz80_build && gunzip <../libz80-2.1.0.tar.gz |tar xvf -)
(cd libz80_build/libz80 && make)
mkdir --parents include lib
cp libz80_build/libz80/z80.h include
cp libz80_build/libz80/libz80.so lib
