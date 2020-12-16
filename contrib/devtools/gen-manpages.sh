#!/usr/bin/env bash
# Copyright (c) 2016-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C
TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
BUILDDIR=${BUILDDIR:-$TOPDIR}

BINDIR=${BINDIR:-$BUILDDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

ELCASHD=${ELCASHD:-$BINDIR/elcashd}
ELCASHCLI=${ELCASHCLI:-$BINDIR/elcash-cli}
BITCOINTX=${BITCOINTX:-$BINDIR/elcash-tx}
WALLET_TOOL=${WALLET_TOOL:-$BINDIR/elcash-wallet}
ELCASHQT=${ELCASHQT:-$BINDIR/qt/elcash-qt}

[ ! -x $ELCASHD ] && echo "$ELCASHD not found or not executable." && exit 1

# The autodetected version git tag can screw up manpage output a little bit
read -r -a ELCASHVER <<< "$($ELCASHCLI --version | head -n1 | awk -F'[ -]' '{ print $6, $7 }')"

# Create a footer file with copyright content.
# This gets autodetected fine for elcashd if --version-string is not set,
# but has different outcomes for elcash-qt and elcash-cli.
echo "[COPYRIGHT]" > footer.h2m
$ELCASHD --version | sed -n '1!p' >> footer.h2m

for cmd in $ELCASHD $ELCASHCLI $BITCOINTX $WALLET_TOOL $ELCASHQT; do
  cmdname="${cmd##*/}"
  help2man -N --version-string=${ELCASHVER[0]} --include=footer.h2m -o ${MANDIR}/${cmdname}.1 ${cmd}
  sed -i "s/\\\-${ELCASHVER[1]}//g" ${MANDIR}/${cmdname}.1
done

rm -f footer.h2m
