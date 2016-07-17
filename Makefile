.PHONY: all config iso docker clean distclean gitclean

all: vmlinuz initramfs.cpio.gz

config: linux/.config

iso: bakka.iso

docker: bakka.tar

clean:
	$(MAKE) -C base clean
	rm -rf rootfs vmlinuz initramfs.cpio.gz isolinux/vmlinuz isolinux/initramfs.cpio.gz bakka.iso bakka.tar

distclean: clean
	$(MAKE) -C linux clean

configclean: distclean
	rm -rf linux/.config

gitclean: clean
	git clean -dffx && git checkout -f
	cd linux && git clean -dffx && git checkout -f
	cd sbase && git clean -dffx && git checkout -f
	cd bash && git clean -dffx && git checkout -f

linux/.config:
	$(MAKE) -C linux defconfig

linux/arch/x86/boot/bzImage: linux/.config
	$(MAKE) -C linux bzImage

sbase/sbase-box:
	$(MAKE) -C sbase CC=musl-gcc LDFLAGS=-static DESTDIR=../rootfs PREFIX=/ sbase-box

bash/bash:
	cd bash && ./configure --enable-static-link --without-bash-malloc --without-libiconv-prefix --without-libintl-prefix CC=musl-gcc CFLAGS=-static
	$(MAKE) -C bash

rootfs: linux/arch/x86/boot/bzImage sbase/sbase-box bash/bash
	mkdir -p $@/bin $@/dev $@/proc $@/root $@/sys $@/tmp
	$(MAKE) -C sbase CC=musl-gcc LDFLAGS=-static DESTDIR=../rootfs PREFIX=/ sbase-box-install
	$(MAKE) -C base
	rm -rf $@/share
	cp bash/bash $@/bin
	ln -s bash $@/bin/sh
	cp base/init $@

vmlinuz: linux/arch/x86/boot/bzImage
	cp $^ $@

initramfs.cpio.gz: rootfs
	cd $^ && find . | cpio --owner 0:0 -H newc -o | gzip > ../$@

isolinux/vmlinuz: vmlinuz
	cp $^ $@

isolinux/initramfs.cpio.gz: initramfs.cpio.gz
	cp $^ $@

bakka.tar: rootfs
	tar -cf $@ $^ --transform='s/^rootfs\///'

bakka.iso: isolinux/vmlinuz isolinux/initramfs.cpio.gz
	mkisofs -b isolinux.bin -c boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table -l -input-charset default -V bakka -A "bakka" -o $@ isolinux

