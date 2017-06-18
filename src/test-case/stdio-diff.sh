#!/bin/sh
# Exercise the STDIO transport for some end-to-end tests.

tests=$(for i in data/test/input/*; do basename ${i}; done)

for i in ${tests}; do
  in="data/test/input/${i}"
  out="data/test/output/${i}"
  tmp="/tmp/cxxhttp-test-output-${i}"

  printf "running test case '%s': " "${i}"

  cat "${in}" | ./server http:stdio > "$tmp"
  if diff --ignore-all-space -u "$out" "$tmp"; then
    echo "OK"
  else
    exec false
  fi
done

exec true
