#!/bin/sh
set -e

# display info about elcash revision
echo "ELcash revision: ${ELCASHD_COMMIT}"

# Prepare configuration from templates
dockerize -template /templates/elcash.conf.tmpl:/elcash/elcash.conf

exec "$@"
