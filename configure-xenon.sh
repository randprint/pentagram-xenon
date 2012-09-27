#instal libSDLdev image, mixer, ttf, and freetype on your host machine then run this and then you should be good to make -f Makefile.xenon
./configure --disable-timidity-midi --disable-alsa --enable-all-hq-scalers --enable-gc-scalers --enable-all-bilinear --disable-debug

echo "hopefully you remembered to install freetype, libSDLdev, SDL_image/mixer/ttf on your host machine before you ran this"
