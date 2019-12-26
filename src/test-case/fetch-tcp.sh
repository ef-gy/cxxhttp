#!/bin/sh
# Test the `fetch` programme over a TCP socket against the `server` programme

port="8081"

rm -f ${socket}
./server "http:localhost:${port}" &
pid=$!
rv="true"

# sleep for a while to make sure the server is initialised.
sleep 1

printf "running test case 1: "

uri="http://localhost:${port}/"
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
    [ -f ${p}.${ex} ] && mv -f ${p}.${ex} fetch-test-hello-${p}.${ex}
  done
done

printf "running test case 2: "

uri="http://localhost:${port}/404"
out="data/test/fetch/404-tcp"
tmp="/tmp/cxxhttp-fetch-test-404-tcp"

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

kill -KILL ${pid}

for ex in gcda gcno; do
  for p in server fetch; do
    [ -f ${p}.${ex} ] && mv -f ${p}.${ex} fetch-test-404-tcp-${p}.${ex}
  done
done

printf "running test case 3: "

uri="http://localhost:${port}/"
out="data/test/fetch/no-connection"
tmp="/tmp/cxxhttp-fetch-test-no-connection"

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
    [ -f ${p}.${ex} ] && mv -f ${p}.${ex} fetch-test-no-connection-${p}.${ex}
  done
done

printf "running test case 4: "

uri="http://localhost-but-actually-not-really:${port}/"
out="data/test/fetch/bad-host"
tmp="/tmp/cxxhttp-fetch-test-bad-host"

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
    [ -f ${p}.${ex} ] && mv -f ${p}.${ex} fetch-test-bad-host-${p}.${ex}
  done
done

exec ${rv}
