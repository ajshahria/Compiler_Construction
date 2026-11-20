# Tricora: the web UI and the three compilers in one container.
#
#   docker build -t tricora .
#   docker run -p 8000:8000 tricora        then open http://localhost:8000
#
# Render (render.yaml) builds this file on every push and sets $PORT.

# ---- 1. React + Monaco frontend -> frontend/dist ----
FROM node:20-slim AS frontend
WORKDIR /app/frontend
COPY frontend/package.json frontend/package-lock.json ./
RUN npm ci --no-audit --no-fund
COPY frontend/ ./
RUN npm run build

# ---- 2. The three compilers, built from source and tested ----
FROM debian:bookworm-slim AS compilers
RUN apt-get update \
 && apt-get install -y --no-install-recommends build-essential flex bison \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /app/backend
COPY backend/ ./
# Files from a Windows checkout may have CRLF line endings; build from a clean
# state, link statically (so the runtime image needs no compiler libraries),
# and run every test suite: a failing test stops the deploy.
RUN find . -type f \( -name '*.l' -o -name '*.y' -o -name '*.c' -o -name '*.cpp' -o -name '*.h' \
        -o -name '*.hpp' -o -name '*.sh' -o -name '*.expected' -o -name 'Makefile' \) \
        -exec sed -i 's/\r$//' {} + \
 && rm -rf c-compiler/build c++_compiler/build javacompiler/build \
 && make -C c-compiler   CC="gcc -static" \
 && make -C c++_compiler CXX="g++ -static" \
 && make -C javacompiler CXX="g++ -static" \
 && make -C c-compiler test \
 && make -C c++_compiler test \
 && make -C javacompiler test

# ---- 3. Runtime: FastAPI server + built UI + compilers ----
FROM python:3.12-slim
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1 PORT=8000
WORKDIR /app
COPY server/requirements.txt server/requirements.txt
RUN pip install --no-cache-dir -r server/requirements.txt
COPY server/*.py server/
COPY --from=compilers /app/backend backend
COPY --from=frontend /app/frontend/dist frontend/dist
# Run as an unprivileged user: the server compiles code typed by visitors.
RUN useradd --system --no-create-home tricora
USER tricora
EXPOSE 8000
CMD uvicorn main:app --app-dir server --host 0.0.0.0 --port ${PORT}
