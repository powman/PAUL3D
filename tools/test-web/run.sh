#!/usr/bin/env bash
# Builds ps5/mg_web.cpp for the host (clang in the ps5sd-build container) and checks the access rules with curl:
# open without a key file, 401 with one, login cookie, Bearer header, wrong key, cross-site POST, no CORS header.
#   MSYS_NO_PATHCONV=1 docker run --rm --platform linux/amd64 -v "$PWD:/src:ro" ps5sd-build bash /src/tools/test-web/run.sh
set -u
mkdir -p /tmp/g /data/PAUL3D/{jobs,in,out,logs}
printf 'static const unsigned char mg_cad_html[]={0}; static const unsigned long mg_cad_html_len=1;\n' > /tmp/g/mg_cad_html.h
# the model catalogue, embedded as scripts/build-mg.sh does it (bytes and a length)
{ printf 'static const unsigned char mg_models_txt[]={'; od -An -v -tu1 /src/ps5/models.txt | tr -s ' \n' ',' | sed 's/^,//;s/,$//'
  printf '};\nstatic const unsigned long mg_models_txt_len=%s;\n' "$(wc -c < /src/ps5/models.txt)"; } > /tmp/g/mg_models_txt.h
# The compiler and the header folders can be set: CXX, HTTPLIB_DIR (trellis.cpp's thirdparty/cpp-httplib) and JSON_DIR
# (llama.cpp's vendor/, for nlohmann/json), e.g. in an alpine container: apk add g++ curl bash; CXX=g++ ...
${CXX:-clang++} -std=c++17 -O1 -I /src/ps5 -I /tmp/g -I "${HTTPLIB_DIR:-/src/ps5/third_party/cpp-httplib}" \
  -I "${JSON_DIR:-/src/.deps/llama.cpp/vendor}" /src/tools/test-web/main.cpp /src/ps5/mg_web.cpp /src/ps5/mg_textjob.cpp /src/ps5/mg_download.cpp \
  -o /tmp/web -lpthread || exit 1
/tmp/web > /tmp/web.log 2>&1 &
sleep 1
U=http://127.0.0.1:7870; fail=0
t() { # name expected actual
  if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: expected $2, got $3"; fail=1; fi; }
code() { curl -s -o /dev/null -w '%{http_code}' "$@"; }

rm -f /data/PAUL3D/api-key.txt
t "no key file: status is open"            200 "$(code $U/api/status)"
t "no key file: /api/auth says not required" '{"required":false,"ok":true}' "$(curl -s $U/api/auth)"
t "cross-site POST is refused"             403 "$(code -X POST -H 'Origin: http://evil.example' $U/api/quit)"
t "same-origin POST gets through (404: no such download)" 404 "$(code -X POST -H 'Origin: http://127.0.0.1:7870' $U/api/downloads/x/cancel)"
t "no CORS allow-origin header"            0   "$(curl -s -D - $U/api/status | grep -ci 'access-control-allow-origin')"

printf 'secret123\n' > /data/PAUL3D/api-key.txt
t "with a key: status without it is 401"   401 "$(code $U/api/status)"
t "with a key: quit without it is 401"     401 "$(code -X POST $U/api/quit)"
t "with a key: the page itself loads"      200 "$(code $U/)"
t "with a key: /api/auth is reachable"     '{"required":true,"ok":false}' "$(curl -s $U/api/auth)"
t "wrong Bearer key is 401"                401 "$(code -H 'Authorization: Bearer nope' $U/api/status)"
t "right Bearer key is 200"                200 "$(code -H 'Authorization: Bearer secret123' $U/api/status)"
t "wrong login is 401"                     401 "$(code -F key=nope $U/api/login)"
rm -f /tmp/jar; t "right login is 200" 200 "$(code -c /tmp/jar -F key=secret123 $U/api/login)"
t "the cookie is HttpOnly and SameSite=Strict" 1 "$(grep -c 'HttpOnly' /tmp/jar)"
t "the cookie opens /api/status"           200 "$(code -b /tmp/jar $U/api/status)"
t "the cookie opens /api/auth as ok"       '{"required":true,"ok":true}' "$(curl -s -b /tmp/jar $U/api/auth)"
t "a wrong cookie is 401"                  401 "$(code -H 'Cookie: mgkey=00ff' $U/api/downloads)"
t "the hex of the key as a cookie is the login" 200 "$(code -H 'Cookie: mgkey=736563726574313233' $U/api/downloads)"

rm -f /data/PAUL3D/api-key.txt
t "key file removed: open again"           200 "$(code $U/api/status)"

# ---- a text-only title: the page at / and /cad, the 3D routes are gone
t "the page is at /"                         200 "$(code $U/)"
t "the page is also at /cad (old links)"     200 "$(code $U/cad)"
t "no mesh jobs any more"                    404 "$(code $U/api/jobs)"
t "no mesh upload any more"                  404 "$(code -X POST -F steps=8 $U/api/jobs)"
t "no 3D weights list any more"              404 "$(code $U/api/weights)"
t "no PS5SD pictures any more"               404 "$(code $U/api/ps5sd/pictures)"

# ---- text: POST /api/text, GET /api/text/<id>, /v1/models, /v1/chat/completions (the worker is played by the test)
J='Content-Type: application/json'
rm -rf /data/PAUL3D/models/text /data/PAUL3D/text /data/PS5LM; mkdir -p /data/PAUL3D/text; rm -f /data/PAUL3D/jobs/*
ASK='{"messages":[{"role":"user","content":"hi"}]}'
t "text: no model is 409"                    409 "$(code -H "$J" -d "$ASK" $U/api/text)"
t "text: the error says missing_model"       1   "$(curl -s -H "$J" -d "$ASK" $U/api/text | grep -c '"missing_model"')"
t "text: no model, /v1/models is empty"      '{"data":[],"object":"list"}' "$(curl -s $U/v1/models)"
mkdir -p /data/PS5LM/models; : > /data/PS5LM/models/z.gguf
t "text: PS5LM's model counts"               202 "$(code -H "$J" -d "$ASK" $U/api/text)"
rm -f /data/PAUL3D/jobs/*; mkdir -p /data/PAUL3D/models/text; : > /data/PAUL3D/models/text/q.gguf
t "text: both folders listed"                '{"data":[{"id":"q.gguf","object":"model"},{"id":"z.gguf","object":"model"}],"object":"list"}' "$(curl -s $U/v1/models)"
t "text: PAUL3D's own model is the default"  1   "$(curl -s $U/api/catalog | grep -c '"text_default":"q.gguf"')"
TID=$(curl -s -H "$J" -d "$ASK" $U/api/text | sed -n 's/.*"id":"\([^"]*\)".*/\1/p')
t "text: accepted with an id"                1   "$([ -n "$TID" ] && echo 1 || echo 0)"
t "text: the job file holds the messages"    1   "$(grep -c '"content":"hi"' /data/PAUL3D/jobs/$TID.textjob 2>/dev/null)"
t "text: not JSON is 400"                    400 "$(code -H "$J" -d '{nope' $U/api/text)"
t "text: no messages is 400"                 400 "$(code -H "$J" -d '{"messages":[]}' $U/api/text)"
t "text: queued"                             1   "$(curl -s $U/api/text/$TID | grep -c '"status":"queued"')"
mv /data/PAUL3D/jobs/$TID.textjob /data/PAUL3D/jobs/$TID.textrunning
t "text: loading"                            1   "$(curl -s $U/api/text/$TID | grep -c '"status":"loading"')"
printf 'cube(1' > /data/PAUL3D/text/$TID.txt
t "text: generating, with the text so far"   1   "$(curl -s $U/api/text/$TID | grep -c '"status":"generating".*"text":"cube(1"')"
printf 'cube(1);' > /data/PAUL3D/text/$TID.txt; printf '{"status":"done"}' > /data/PAUL3D/text/$TID.textdone; rm -f /data/PAUL3D/jobs/$TID.textrunning
t "text: done"                               1   "$(curl -s $U/api/text/$TID | grep -c '"status":"done".*"text":"cube(1);"')"
t "text: unknown id is 404"                  404 "$(code $U/api/text/t19990101-000000-0)"
fake_worker() {  # answers the next .textjob after a second, as the title's worker would
  sleep 1; f=$(ls /data/PAUL3D/jobs/*.textjob | head -1); id=$(basename "$f" .textjob); cp "$f" /data/PAUL3D/text/$id.req
  printf '%s' "$1" > /data/PAUL3D/text/$id.txt; printf '{"status":"done"}' > /data/PAUL3D/text/$id.textdone; rm -f "$f"; }
rm -f /data/PAUL3D/jobs/*
fake_worker 'sphere(5);' & fw=$!
R=$(curl -s -m 20 -H "$J" -d '{"model":"x","messages":[{"role":"user","content":"a ball"}]}' $U/v1/chat/completions)
t "chat completions: the answer"             1   "$(grep -c '"content":"sphere(5);"' <<<"$R")"
t "chat completions: OpenAI's shape"         1   "$(grep -c '"finish_reason":"stop"' <<<"$R")"
wait $fw
fake_worker 'cube(2);' & fw=$!
R=$(curl -s -m 20 -H "$J" -d '{"stream":true,"messages":[{"role":"user","content":"a box"}]}' $U/v1/chat/completions)
t "chat completions, stream: the text"       1   "$(grep -c '^data: .*cube(2);' <<<"$R")"
t "chat completions, stream: [DONE]"         1   "$(grep -c '^data: \[DONE\]' <<<"$R")"
wait $fw
printf 'secret123\n' > /data/PAUL3D/api-key.txt
t "text: needs the key"                      401 "$(code -H "$J" -d "$ASK" $U/api/text)"
t "chat completions: needs the key"          401 "$(code -H "$J" -d "$ASK" $U/v1/chat/completions)"
t "v1/models: needs the key"                 401 "$(code $U/v1/models)"
t "chat completions: Bearer key works"       1   "$(curl -s -H 'Authorization: Bearer secret123' $U/v1/models | grep -c q.gguf)"
rm -f /data/PAUL3D/api-key.txt /data/PAUL3D/jobs/*

# ---- downloads: GET /api/catalog, POST /api/downloads, GET /api/downloads, cancel (the worker is not running here)
rm -rf /data/PAUL3D/models /data/PAUL3D/downloads; rm -f /data/PAUL3D/jobs/*
C=$(curl -s $U/api/catalog)
t "catalog: only the text group"            1   "$(grep -o '"group":"[a-z0-9-]*"' <<<"$C" | wc -l)"
t "catalog: nothing installed"               0   "$(grep -o '"installed":true' <<<"$C" | wc -l)"
t "catalog: the text group and its size"     1   "$(grep -o '{[^{}]*"group":"text-qwen35-9b"[^{}]*}' <<<"$C" | grep -c '"size":5680522464')"
t "catalog: the text group says what for"    1   "$(grep -o '{[^{}]*"group":"text-qwen35-9b"[^{}]*}' <<<"$C" | grep -c '"needed_for":"text"')"
t "catalog: a licence link"                  1   "$(grep -o '{[^{}]*"group":"text-qwen35-9b"[^{}]*}' <<<"$C" | grep -c '"licence":"https://huggingface.co/unsloth/Qwen3.5-9B-GGUF"')"
t "catalog: free space is given"             1   "$(grep -c '"free":[0-9]' <<<"$C")"
t "download: without accept_license is 400"  400 "$(code -H "$J" -d '{"group":"text-qwen35-9b"}' $U/api/downloads)"
t "download: unknown group is 404"           404 "$(code -H "$J" -d '{"group":"nope","accept_license":true}' $U/api/downloads)"
DID=$(curl -s -H "$J" -d '{"group":"text-qwen35-9b","accept_license":true}' $U/api/downloads | sed -n 's/.*"id":"\([^"]*\)".*/\1/p')
t "download: accepted with an id"            1   "$([ -n "$DID" ] && echo 1 || echo 0)"
t "download: the job file is there"          1   "$(grep -c '"group":"text-qwen35-9b"' /data/PAUL3D/jobs/$DID.dljob 2>/dev/null)"
t "download: the same group again is 409"    409 "$(code -H "$J" -d '{"group":"text-qwen35-9b","accept_license":true}' $U/api/downloads)"
t "downloads: listed as queued"              1   "$(curl -s $U/api/downloads | grep -c "\"id\":\"$DID\".*\"state\":\"queued\"")"
t "downloads: cancel"                        200 "$(code -X POST $U/api/downloads/$DID/cancel)"
t "downloads: cancelled, job file gone"      1   "$(curl -s $U/api/downloads | grep -c '"state":"cancelled"')$([ -f /data/PAUL3D/jobs/$DID.dljob ] && echo x)"
t "downloads: cancel of an unknown id 404"   404 "$(code -X POST $U/api/downloads/d19990101-000000-0/cancel)"
mkdir -p /data/PAUL3D/models/text; truncate -s 5680522464 /data/PAUL3D/models/text/Qwen3.5-9B-Q4_K_M.gguf
t "catalog: a file in full is installed"     1   "$(curl -s $U/api/catalog | grep -o '{[^{}]*"group":"text-qwen35-9b"[^{}]*}' | grep -c '"installed":true')"
t "download: an installed group is 409"      409 "$(code -H "$J" -d '{"group":"text-qwen35-9b","accept_license":true}' $U/api/downloads)"
# ---- several text models: listed, chosen per request, shown in the catalogue
rm -f /data/PAUL3D/jobs/*; mkdir -p /data/PAUL3D/models/text; : > /data/PAUL3D/models/text/Qwen3.8-27B-UD-Q2_K_XL.gguf
t "models: all installed ones"               '{"data":[{"id":"Qwen3.5-9B-Q4_K_M.gguf","object":"model"},{"id":"Qwen3.8-27B-UD-Q2_K_XL.gguf","object":"model"},{"id":"z.gguf","object":"model"}],"object":"list"}' "$(curl -s $U/v1/models)"
C=$(curl -s $U/api/catalog)
t "catalog: the text models"                 1   "$(grep -c '"text_models":\["Qwen3.5-9B-Q4_K_M.gguf","Qwen3.8-27B-UD-Q2_K_XL.gguf","z.gguf"\]' <<<"$C")"
t "catalog: the default one"                 1   "$(grep -c '"text_default":"Qwen3.5-9B-Q4_K_M.gguf"' <<<"$C")"
MID=$(curl -s -H "$J" -d '{"model":"Qwen3.8-27B-UD-Q2_K_XL.gguf","messages":[{"role":"user","content":"hi"}]}' $U/api/text | sed -n 's/.*"id":"\([^"]*\)".*/\1/p')
t "text: a chosen model goes in the job"     1   "$(grep -c '"model":"Qwen3.8-27B-UD-Q2_K_XL.gguf"' /data/PAUL3D/jobs/$MID.textjob 2>/dev/null)"
t "text: an unknown model is 404"            404 "$(code -H "$J" -d '{"model":"nope.gguf","messages":[{"role":"user","content":"hi"}]}' $U/api/text)"
rm -f /data/PAUL3D/jobs/* /data/PAUL3D/text/*.req
fake_worker 'ok' & fw=$!
curl -s -m 20 -H "$J" -d '{"model":"gpt-4o","messages":[{"role":"user","content":"x"}]}' $U/v1/chat/completions > /dev/null
wait $fw
t "chat completions: a foreign model name is left out" 0 "$(cat /data/PAUL3D/text/*.req 2>/dev/null | grep -c 'gpt-4o')"
t "chat completions: the request reached the worker" 1 "$(ls /data/PAUL3D/text/*.req 2>/dev/null | wc -l | awk '{print ($1>0)}')"
rm -f /data/PAUL3D/models/text/Qwen3.8-27B-UD-Q2_K_XL.gguf /data/PAUL3D/jobs/*

printf 'secret123\n' > /data/PAUL3D/api-key.txt
t "catalog: needs the key"                   401 "$(code $U/api/catalog)"
t "downloads: needs the key"                 401 "$(code $U/api/downloads)"
rm -f /data/PAUL3D/api-key.txt /data/PAUL3D/jobs/*; rm -rf /data/PAUL3D/models/text

kill %1 2>/dev/null
[ $fail = 0 ] && echo "ALL PASSED" || echo "SOME FAILED"
exit $fail
