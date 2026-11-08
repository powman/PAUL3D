# Web API (port 7870)

| Route | |
|---|---|
| `GET /` (and `/cad`) | the page: a part from a sentence, and Modelos |
| `GET /api/status` | `busy`, `job`, `stage` (`carregando <model>`, `pronto: <model>`, `respondendo: <model>`, a download), `elapsed`, pool memory, threads, log tail |
| `POST /api/text` | JSON `{"messages":[{"role","content"}...],"model"?,"max_tokens"?,"temperature"?}` → `202 {"id"}`. `model` is a file name from `/v1/models`; without it, the one loaded. `409 {"error":"missing_model"}` without any model, `404` for an unknown model, `400` for a bad request |
| `GET /api/text/<id>` | `{"id","status":"queued"\|"loading"\|"generating"\|"done"\|"failed","text","error"?}`; `text` grows while it is made |
| `GET /v1/models` | every installed `.gguf` (`/data/PAUL3D/models/text`, then PS5LM's `/data/PS5LM/models`) |
| `POST /v1/chat/completions` | OpenAI's API on the same queue; a model name that is not installed means the loaded one; the answer comes when it is done (with `"stream":true` as one event) |
| `GET /api/catalog` | the models the title can download (`ps5/models.txt`): `{"groups":[{"group","title","size","have","installed","licence","needed_for"}],"free","text_models","text_default"}` |
| `POST /api/downloads` | JSON `{"group","accept_license":true}` → `202 {"id"}`; `400` without `accept_license`, `404` unknown group, `409` installed or already queued |
| `GET /api/downloads` | the downloads, newest first: `state` (`queued`, `downloading`, `verifying`, `done`, `failed`, `cancelled`), `done`, `total`, `speed` (bytes/s), `error` |
| `POST /api/downloads/<id>/cancel` | stops it (the part already downloaded stays for next time) |
| `GET /api/auth`, `POST /api/login` | the optional key (`/data/PAUL3D/api-key.txt`): `{"required","ok"}`; login with the form field `key` sets a cookie |
| `POST /api/quit` | the title ends itself |

With a key, every route but `/`, `/cad`, `/api/auth` and `/api/login` needs it: the cookie of the page, or
`Authorization: Bearer <key>`. A POST from another site is refused (403).

```sh
curl -H 'Content-Type: application/json' -d '{"messages":[{"role":"user","content":"OpenSCAD: a 20 mm cube with a 5 mm hole"}]}' \
  http://<console>:7870/v1/chat/completions
```
