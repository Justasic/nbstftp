pkgname=nbstftp
pkgver=1.0.0
pkgrel=1
pkgdesc="A simple to use No Bullshit TFTP server"
arch=(i686 x86_64)
license=(BSD)
makedepends=('git' 'flex' 'bison' 'gcc>=4.7.0')
url="https://github.com/justasic/nbstftp"
#options=('!emptydirs')
# install=nbstftp.install
provides=('nbstftp' 'nbstftp-git')
source=("nbstftp::git+https://github.com/Justasic/nbstftp.git")
sha1sums=('SKIP')

pkgver() {
	cd nbstftp
	echo $pkgver.$(git rev-parse --short HEAD)
}

prepare() {
	cd nbstftp
	mkdir build
	cd build

	msg2 "Generating build..."
	#cmake -DNO_CLANG=BOOLEAN:TRUE -DCMAKE_BUILD_TYPE:STRING=RELEASE -DCMAKE_INSTALL_PREFIX:STRING=/usr build
	cmake -DNO_CLANG=BOOLEAN:TRUE -DCMAKE_BUILD_TYPE:STRING=RELEASE ..
}

build() {
	rm -rf "nbstftp/build/*"
	cd "nbstftp/build"
	msg2 "Building..."
	make
}

package() {
	cd "nbstftp/build"
	msg2 "Installing..."
	make DESTDIR="$pkgdir" install

	gzip < ../doc/nbstftp.8 > nbstftp.8.gz

	# Install all the license files
	install -Dm644 ../doc/LICENSE ${pkgdir}/usr/share/licenses/${pkgname}/LICENSE
	install -Dm644 ../doc/LICENSE.reallocarray ${pkgdir}/usr/share/licenses/${pkgname}/LICENSE.reallocarray
	install -Dm644 ../doc/LICENSE.vec ${pkgdir}/usr/share/licenses/${pkgname}/LICENSE.vec
	install -Dm644 nbstftp.8.gz ${pkgdir}/usr/share/man8/nbstftp.8.gz
	install -Dm644 ../doc/nbstftp.service ${pkgdir}/usr/lib/systemd/system/nbstftp.service
}
