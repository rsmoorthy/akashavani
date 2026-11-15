FROM python:3.12-slim AS builder

ENV PIP_DISABLE_PIP_VERSION_CHECK=1
WORKDIR /build

COPY pyproject.toml README.md ./
COPY src ./src
RUN pip install --no-cache-dir build \
    && python -m build --wheel --outdir /dist

FROM python:3.12-slim

ENV PYTHONUNBUFFERED=1 \
    PIP_DISABLE_PIP_VERSION_CHECK=1 \
    PIP_NO_CACHE_DIR=1

ARG UID=1000
ARG GID=1000

RUN groupadd --gid "${GID}" app \
    && useradd --uid "${UID}" --gid app --create-home --shell /usr/sbin/nologin app

WORKDIR /app

COPY --from=builder /dist /dist
RUN pip install /dist/*.whl "psycopg[binary]>=3.2,<4" && rm -rf /dist

COPY alembic.ini ./
COPY migrations ./migrations
COPY scripts/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod 0755 /usr/local/bin/entrypoint.sh \
    && mkdir -p /data/firmware \
    && chown -R app:app /app /data

USER app

EXPOSE 8000

HEALTHCHECK --interval=30s --timeout=5s --start-period=15s --retries=3 \
  CMD python -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8000/healthz').read()"

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["uvicorn", "akashavani_server.main:app", "--host", "0.0.0.0", "--port", "8000"]
