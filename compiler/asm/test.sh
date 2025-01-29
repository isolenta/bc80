#!/bin/bash

# usage test.sh /path/to/as /path/to/disas [test]

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

SCRIPTNAME=`realpath $0`
DIRNAME=`dirname $SCRIPTNAME`

TMPDIR=/tmp/asmtest
MY_AS=$1
MY_DISAS=$2
EXT_AS=z80asm

INPUTDIR=${DIRNAME}/tests
LOGFILE=${DIRNAME}/test.log

rm -rf $TMPDIR
mkdir -p $TMPDIR
rm -f $LOGFILE

total=0
failed=0

for infile in $INPUTDIR/*.asm; do
  BASENAME=`basename $infile`

  if [ "$3" != "" ]; then
    if [ "$BASENAME" != "$3" ]; then
      continue
    fi
  fi

  echo -n "${infile}... "
  total=$((total+1))

  # 1. Assembly test source
  echo "${MY_AS} -t raw -o ${TMPDIR}/${BASENAME}.step1.bin $infile" >> $LOGFILE
  ${MY_AS} -t raw -o ${TMPDIR}/${BASENAME}.step1.bin $infile >> $LOGFILE 2>&1
  if [ "$?" != "0" ]; then
    echo -e "${RED}Failed${NC}"
    failed=$((failed+1))
    continue
  fi

  # 2. Disassemble produced binary
  echo "${MY_DISAS} ${TMPDIR}/${BASENAME}.step1.bin > ${TMPDIR}/${BASENAME}.step2.asm" >> $LOGFILE
  ${MY_DISAS} -l ${TMPDIR}/${BASENAME}.step1.bin > ${TMPDIR}/${BASENAME}.step2.asm 2>> $LOGFILE
  if [ "$?" != "0" ]; then
    echo -e "${RED}Failed${NC}"
    failed=$((failed+1))
    continue
  fi

  # 3. Assembly previously disassembled file
  echo "${MY_AS} -t raw -o ${TMPDIR}/${BASENAME}.step3.bin ${TMPDIR}/${BASENAME}.step2.asm" >> $LOGFILE
  ${MY_AS} -t raw -o ${TMPDIR}/${BASENAME}.step3.bin ${TMPDIR}/${BASENAME}.step2.asm >> $LOGFILE 2>&1
  if [ "$?" != "0" ]; then
    echo -e "${RED}Failed${NC}"
    failed=$((failed+1))
    continue
  fi

  # 4. Compare binaries
  diff ${TMPDIR}/${BASENAME}.step3.bin ${TMPDIR}/${BASENAME}.step1.bin >> $LOGFILE 2>&1
  if [ "$?" != "0" ]; then
    echo -e "${RED}Failed${NC}"
    failed=$((failed+1))
    continue
  fi

  if ! grep -q "skip-3rdparty" "${infile}"; then
    # 5. Assembly with 3rdparty assembler
    echo "${EXT_AS} -i $infile -o ${TMPDIR}/${BASENAME}.step4.bin" >> $LOGFILE
    ${EXT_AS} -i $infile -o ${TMPDIR}/${BASENAME}.step4.bin  >> $LOGFILE 2>&1

    # 6. Compare binaries
    diff ${TMPDIR}/${BASENAME}.step3.bin ${TMPDIR}/${BASENAME}.step4.bin >> $LOGFILE 2>&1
    if [ "$?" != "0" ]; then
      echo -e "${RED}Failed${NC}"
      failed=$((failed+1))
      continue
    fi
  fi

  echo -e "${GREEN}OK${NC}"
done

echo "--"
echo "Total tests passed: ${total}"
if [ "$failed" -ne "0" ]; then
  echo -e "${RED}Failed: ${failed}${NC}"
fi
