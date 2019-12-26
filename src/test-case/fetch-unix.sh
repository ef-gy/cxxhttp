#!/bin/sh
# Test the `fetch` programme over a UNIX socket against the `server` programme

socket="/tmp/cxxhttp-fetch-test-socket"

./server "http:unix:${socket}" &
pid=$!
rv="true"

# sleep for a while to make sure the server is initialised.
sleep 1

printf "running test case 1: "

uri="http:unix:${socket}:/"
out="data/test/fetch/hello"
tmp="/tmp/cxxhttp-fetch-test-hello"

if ./fetch "${uri}" > "${tmp}"; then
  if diff --ignore-all-space -u "${out}" "${tmp}"; then
    echo "OK"
  else
    echo "FAIL"
    rv="false"
  fi
else
  echo "FAIL"
  rv="false"
fi

for ex in gcda gcno; do
  for p in server fetch; do
    [ -f ${p}.${ex} ] && mv -f ${p}.${ex} fetch-test-hello-unix-${p}.${ex}
  done
done

printf "running test case 2: "

uri="http:unix:${socket}:/404"
out="data/test/fetch/404-unix"
tmp="/tmp/cxxhttp-fetch-test-404-unix"

if ./fetch "${uri}" > "${tmp}" 2>&1; then
  if diff --ignore-all-space -u "${out}" "${tmp}"; then
    echo "OK"
  else
    echo "FAIL"
    rv="false"
  fi
else
  echo "FAIL"
  rv="false"
fi

for ex in gcda gcno; do
  for p in server fetch; do
    [ -f ${p}.${ex} ] && mv -f ${p}.${ex} fetch-test-404-unix-${p}.${ex}
  done
done

kill -KILL ${pid}

exec ${rv}
