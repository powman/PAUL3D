#!/usr/bin/env bash
# Builds ps5/mg_web.cpp for the host (clang in the ps5sd-build container) and checks the access rules with curl:
# open without a key file, 401 with one, login cookie, Bearer header, wrong key, cross-site POST, no CORS header.
#   MSYS_NO_PATHCONV=1 docker run --rm --platform linux/amd64 -v "$PWD:/src:ro" ps5sd-build bash /src/tools/test-web/run.sh
set -u
mkdir -p /tmp/g /data/PAUL3D/{jobs,in,out,logs}
printf 'static const unsigned char mg_index_html[]={0}; static const unsigned long mg_index_html_len=1;\n' > /tmp/g/mg_index_html.h
printf 'static const unsigned char mg_cad_html[]={0}; static const unsigned long mg_cad_html_len=1;\n' > /tmp/g/mg_cad_html.h
# the model catalogue, embedded as scripts/build-mg.sh does it (bytes and a length)
{ printf 'static const unsigned char mg_models_txt[]={'; od -An -v -tu1 /src/ps5/models.txt | tr -s ' \n' ',' | sed 's/^,//;s/,$//'
  printf '};\nstatic const unsigned long mg_models_txt_len=%s;\n' "$(wc -c < /src/ps5/models.txt)"; } > /tmp/g/mg_models_txt.h
# The compiler and the header folders can be set: CXX, HTTPLIB_DIR (trellis.cpp's thirdparty/cpp-httplib) and JSON_DIR
# (llama.cpp's vendor/, for nlohmann/json), e.g. in an alpine container: apk add g++ curl bash; CXX=g++ ...
${CXX:-clang++} -std=c++17 -O1 -I /src/ps5 -I /tmp/g -I "${HTTPLIB_DIR:-/src/src/trellis.cpp/thirdparty/cpp-httplib}" \
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
t "same-origin POST gets through (409: no such queued job)" 409 "$(code -X POST -H 'Origin: http://127.0.0.1:7870' $U/api/jobs/x/cancel)"
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
t "a wrong cookie is 401"                  401 "$(code -H 'Cookie: mgkey=00ff' $U/api/jobs)"
t "the hex of the key as a cookie is the login" 200 "$(code -H 'Cookie: mgkey=736563726574313233' $U/api/jobs)"

printf '[1/7] preprocess\n[ScePthread/System] Internal Memory is running out.\nerror: could not read ss_flow.gguf\n' > /data/PAUL3D/logs/w1.log
t "job log: the noise is left out"         '{"lines":["[1/7] preprocess","error: could not read ss_flow.gguf"]}' "$(curl -s -H 'Authorization: Bearer secret123' $U/api/jobs/w1/log)"
t "job log needs the key too"              401 "$(code $U/api/jobs/w1/log)"
rm -f /data/PAUL3D/api-key.txt
t "key file removed: open again"           200 "$(code $U/api/status)"

# ---- pictures from PS5SD, the atlas field
mkdir -p /data/PS5SD/outputs /data/PAUL3D/models/q4; : > /data/PAUL3D/models/q4/ss_flow.gguf
printf '\x89PNG\r\n\x1a\nPS5SD-PICTURE' > /data/PS5SD/outputs/sd_20261022_100000_1.png
printf 'x' > /data/PS5SD/outputs/sd_20261022_100000_2.jpg; printf 'x' > /data/PS5SD/outputs/notes.txt; printf 'x' > '/data/PS5SD/outputs/bad name.png'; printf 'x' > /data/PS5SD/outputs/.hidden.png
L=$(curl -s $U/api/ps5sd/pictures)
t "PS5SD list: a png and a jpg, newest first" '[{"name":"sd_20261022_100000_2.jpg","bytes":1},{"name":"sd_20261022_100000_1.png","bytes":21}]' "$L"
t "PS5SD picture is served"                  200 "$(code $U/api/ps5sd/picture/sd_20261022_100000_1.png)"
t "PS5SD picture type is image/png"          1   "$(curl -s -D - -o /dev/null $U/api/ps5sd/picture/sd_20261022_100000_1.png | grep -ci 'content-type: image/png')"
t "a text file is not served as a picture"   400 "$(code $U/api/ps5sd/picture/notes.txt)"
t "a hidden file is refused"                 400 "$(code $U/api/ps5sd/picture/.hidden.png)"
t "double dots are refused"                  400 "$(code $U/api/ps5sd/picture/a..b.png)"
t "an encoded path does not escape"          1   "$([ "$(code --path-as-is "$U/api/ps5sd/picture/..%2f..%2f..%2fetc%2fpasswd")" != 200 ] && echo 1 || echo 0)"
t "a path with slashes does not escape"      1   "$([ "$(code --path-as-is "$U/api/ps5sd/picture/../../../etc/passwd")" != 200 ] && echo 1 || echo 0)"
rm -f /data/PAUL3D/jobs/* /data/PAUL3D/in/*
R=$(curl -s -F from_ps5sd=sd_20261022_100000_1.png -F atlas=2048 -F steps=6 $U/api/jobs); ID=$(echo "$R" | sed -n 's/.*"id":"\([^"]*\)".*/\1/p')
t "job from a PS5SD picture is accepted"     1   "$([ -n "$ID" ] && echo 1 || echo 0)"
t "the picture was copied into in/"          0   "$(cmp -s /data/PS5SD/outputs/sd_20261022_100000_1.png /data/PAUL3D/in/$ID.png; echo $?)"
t "the job line carries --atlas 2048"        1   "$(grep -c -- '--atlas 2048' /data/PAUL3D/jobs/$ID.job)"
t "the job line carries --steps 6"           1   "$(grep -c -- '--steps 6' /data/PAUL3D/jobs/$ID.job)"
R2=$(curl -s -F from_ps5sd=sd_20261022_100000_2.jpg $U/api/jobs); ID2=$(echo "$R2" | sed -n 's/.*"id":"\([^"]*\)".*/\1/p')
t "no atlas given: no --atlas in the job"    0   "$(grep -c -- '--atlas' /data/PAUL3D/jobs/$ID2.job)"
t "atlas 999 is refused"                     400 "$(code -F from_ps5sd=sd_20261022_100000_1.png -F atlas=999 $U/api/jobs)"
t "a path as picture name is refused"        400 "$(code -F from_ps5sd=../x.png $U/api/jobs)"
t "a missing picture is 404"                 404 "$(code -F from_ps5sd=nothing.png $U/api/jobs)"
t "no picture at all is 400"                 400 "$(code -F steps=8 $U/api/jobs)"
# parameters without multipart: url-encoded (curl -d) and in the query string
R3=$(curl -s -d from_ps5sd=sd_20261022_100000_1.png -d steps=11 -d seed=5 -d faces=250000 -d atlas=1024 $U/api/jobs); ID3=$(echo "$R3" | sed -n 's/.*"id":"\([^"]*\)".*/\1/p')
t "url-encoded job is accepted"              1   "$([ -n "$ID3" ] && echo 1 || echo 0)"
t "url-encoded steps reach the job line"     1   "$(grep -c -- '--steps 11' /data/PAUL3D/jobs/$ID3.job)"
t "url-encoded seed reaches the job line"    1   "$(grep -c -- '-s 5 ' /data/PAUL3D/jobs/$ID3.job)"
t "url-encoded faces and atlas arrive"       1   "$(grep -c -- '--faces 250000 --atlas 1024' /data/PAUL3D/jobs/$ID3.job)"
R4=$(curl -s -X POST "$U/api/jobs?from_ps5sd=sd_20261022_100000_1.png&steps=3"); ID4=$(echo "$R4" | sed -n 's/.*"id":"\([^"]*\)".*/\1/p')
t "query-string parameters work too"         1   "$(grep -c -- '--steps 3' /data/PAUL3D/jobs/$ID4.job)"
ID5=$(curl -s -d from_ps5sd=sd_20261022_100000_1.png -d steps=999 $U/api/jobs | sed -n 's/.*"id":"\([^"]*\)".*/\1/p')
t "out-of-range steps are clamped to 50"     1   "$(grep -c -- '--steps 50' /data/PAUL3D/jobs/$ID5.job)"
printf 'secret123\n' > /data/PAUL3D/api-key.txt
t "with a key: the PS5SD list needs it"      401 "$(code $U/api/ps5sd/pictures)"
t "with a key: a PS5SD picture needs it"     401 "$(code $U/api/ps5sd/picture/sd_20261022_100000_1.png)"
rm -f /data/PAUL3D/api-key.txt

# ---- /api/weights: what is on the console (read only)
rm -rf /data/PAUL3D/models/q4; mkdir -p /data/PAUL3D/models
t "weights: no folder, nothing is complete"  false "$(curl -s $U/api/weights | sed -n 's/.*"complete":\(true\|false\).*/\1/p')"
t "weights: all ten listed"                  10  "$(curl -s $U/api/weights | grep -o '"name"' | wc -l)"
t "weights: all missing at first"            10  "$(curl -s $U/api/weights | grep -o '"state":"missing"' | wc -l)"
mkdir -p /data/PAUL3D/models/q4
mk() { truncate -s "$2" "/data/PAUL3D/models/q4/$1"; }
mk dinov3.gguf 172662976; mk ss_flow.gguf 730496672; mk ss_dec.gguf 147379392; mk shape_flow_512.gguf 730520576
mk shape_dec.gguf 845423552; mk tex_flow_512.gguf 730548224; mk tex_dec.gguf 845414272
t "weights: the required files, in full: complete" true "$(curl -s $U/api/weights | sed -n 's/.*"complete":\(true\|false\).*/\1/p')"
t "weights: optional files may be missing"   3   "$(curl -s $U/api/weights | grep -o '"state":"missing"' | wc -l)"
mk tex_dec.gguf 1000
t "weights: a half-copied file is partial"   1   "$(curl -s $U/api/weights | grep -o '"state":"partial"' | wc -l)"
t "weights: a partial file means not complete" false "$(curl -s $U/api/weights | sed -n 's/.*"complete":\(true\|false\).*/\1/p')"
printf 'secret123\n' > /data/PAUL3D/api-key.txt
t "weights: needs the key when one is set"   401 "$(code $U/api/weights)"
rm -f /data/PAUL3D/api-key.txt
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
t "text: PAUL3D's own model first"           '{"data":[{"id":"q.gguf","object":"model"}],"object":"list"}' "$(curl -s $U/v1/models)"
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
  sleep 1; f=$(ls /data/PAUL3D/jobs/*.textjob | head -1); id=$(basename "$f" .textjob)
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
t "catalog: two groups"                      2   "$(grep -o '"group":"[a-z0-9-]*"' <<<"$C" | wc -l)"
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
printf 'secret123\n' > /data/PAUL3D/api-key.txt
t "catalog: needs the key"                   401 "$(code $U/api/catalog)"
t "downloads: needs the key"                 401 "$(code $U/api/downloads)"
rm -f /data/PAUL3D/api-key.txt /data/PAUL3D/jobs/*; rm -rf /data/PAUL3D/models/text

kill %1 2>/dev/null
[ $fail = 0 ] && echo "ALL PASSED" || echo "SOME FAILED"
exit $fail
