make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- uImage -j8

cd /home/falinux/work/nxp_zeroboot/out
sudo sh loldown.sh
cd -

