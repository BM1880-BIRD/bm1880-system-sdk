# modify rootfs/etc/fstab to mount /dev as tmpfs
sed -i '4,5d' etc/fstab
sed -i '1d' etc/fstab
sed -i '3 a tmpfs\t\t/dev\t\ttmpfs\tdefaults\t0\t0' etc/fstab
sed -i '3 a tmpfs\t\t/var\/log\t\ttmpfs\tdefaults\t0\t0' etc/fstab
sed -i '3 a tmpfs\t\t/var\/empty\t\ttmpfs\tdefaults\t0\t0' etc/fstab
echo 'cat etc/fstab'
cat etc/fstab

sed -i '3,4d' etc/init.d/S02dev
#cat etc/init.d/S02dev

#echo 'cat S50sshd'
sed -i '14,15d' etc/init.d/S50sshd
sed -i '8 a chmod 0600 /var/empty' etc/init.d/S50sshd
#cat etc/init.d/S50sshd

sed -i '9d' etc/init.d/S51daemonip

#ls etc/init.d

chmod 0600 usr/local/etc/*_key
chmod 0600 usr/local/etc/*.pub
#ls -al usr/local/etc/

sed -i 's/\/etc\/resolv.conf/\/run\/resolv.conf/g' usr/default.script
sed -i 's/\/etc\/resolv.conf/\/run\/resolv.conf/g' etc/init.d/S40network
sed -i 's/\/etc\/resolv.conf/\/run\/resolv.conf/g' usr/share/udhcpc/default.script

# modify rootfs/init to remove busybox install
cp init_ramboot.sh.sqsh init
rm init_*sh

mkdir mnt/system
mkdir mnt/data
