# Maintainer: Harpiah <seu-email@provedor.com>
# Protognum: Gerenciador Tático Alpha Node

pkgname=protofull-git
_pkgname=protofull
pkgver=1.0.r10 # r10 indica o número de commits, o makepkg ajusta isso
pkgrel=1
pkgdesc="Gerenciador Tático Alpha Node - Central de Comando Archon"
arch=('x86_64')
url="https://github.com"
license=('MIT')

# Adicionei 'udisks2' e 'polkit', essenciais para a montagem de USB que você usa
depends=('ncurses' 'networkmanager' 'qterminal' 'nsxiv' 'udisks2' 'polkit')
makedepends=('gcc' 'make' 'git')
provides=('protognum')
conflicts=('protognum')

# Puxa o código direto do seu GitHub
source=("git+${url}.git")
sha256sums=('SKIP')

build() {
  cd "$_pkgname"
  # Garante que as pastas bin/ e obj/ existam antes de compilar
  make prepare || mkdir -p bin obj
  make
}

package() {
  cd "$_pkgname"
  # Instala o binário forjado pelo Makefile em /usr/bin
  install -Dm755 bin/protognum "${pkgdir}/usr/bin/protognum"
  
  # Instala a licença (importante para repositórios públicos)
  install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/$pkgname/LICENSE"
}
