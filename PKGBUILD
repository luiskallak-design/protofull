# Maintainer: Harpiah <seu-email@provedor.com>
# Protofull: Gerenciador Tático Alpha Node

pkgname=protofull-git
_pkgname=protofull
pkgver=1.0.r10
pkgrel=1
pkgdesc="Gerenciador Tático Alpha Node - Central de Comando Archon (Gerenciador de arquivos e lançador TUI)"
arch=('x86_64')
url="https://github.com/luiskallak-design/protofull"
license=('MIT')

# Mantidas as dependências originais e acrescentadas as táticas do sistema
depends=('ncurses' 'networkmanager' 'qterminal' 'nsxiv' 'udisks2' 'polkit' 'alsa-utils' 'ffmpeg' 'mpv' 'libx11' 'libxft')
makedepends=('gcc' 'make' 'git' 'pkg-config')
provides=('protofull')
conflicts=('protofull' 'protognum-git' 'protognum')

# Puxando direto do link oficial que você mandou
source=("git+${url}.git")
sha256sums=('SKIP')

build() {
  cd "$_pkgname"
  make prepare || mkdir -p bin obj
  make
}

package() {
  cd "$_pkgname"
  # Proteção para aceitar qualquer um dos dois nomes gerados no Makefile
  if [ -f bin/protofull ]; then
    install -Dm755 bin/protofull "${pkgdir}/usr/bin/protofull"
  else
    install -Dm755 bin/protognum "${pkgdir}/usr/bin/protofull"
  fi
  
  # Garante a instalação da licença MIT no sistema
  install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/$pkgname/LICENSE"
}

