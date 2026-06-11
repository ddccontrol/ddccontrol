# Copyright(c) 2018 Miroslav Kravec (kravec.miroslav@gmail.com)
# Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)
# SPDX-License-Identifier: GPL-2.0

if [ "$EUID" -ne 0 ];
then
    echo 'Run as root'
    exit 1
fi

# environment variables
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$( dirname "${DIR}" )"

source "${DIR}/common/suppressions.sh"
