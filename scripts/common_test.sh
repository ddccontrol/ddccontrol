
[ "$EUID" -ne 0 ] && echo "Run as root" && exit 1

# environment variables
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$( dirname "${DIR}" )"

source "${DIR}/common/suppressions.sh"
