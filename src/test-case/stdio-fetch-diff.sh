#!/bin/sh
# Exercise the STDIO transport for some end-to-end tests.

tests=$(for i in data/test/fetch-input/*; do basename ${i}; done)

for i in ${tests}; do
  uri="data/test/fetch-uri/${i}"
  in="data/test/fetch-input/${i}"
  outp="data/test/fetch-proto/${i}"
  outc="data/test/fetch-output/${i}"
  tmpp="/tmp/cxxhttp-test-fetch-proto-${i}"
  tmpc="/tmp/cxxhttp-test-fetch-output-${i}"

  printf "running test case '%s': " "${i}"

  ./fetch output-fd:3 $(cat ${uri}) <"${in}" >"${tmpp}" 3>"${tmpc}"
  if diff --ignore-all-space -u "${outc}" "${tmpc}"; then
    if diff --ignore-all-space -u "${outp}" "${tmpp}"; then
      echo "OK"
    else
      echo "FAIL"
      exec false
    fi
  else
    echo "FAIL"
    exec false
  fi
done

exec true
