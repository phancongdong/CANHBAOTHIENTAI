FROM python:3.12-slim

# Create non-root system user
RUN groupadd -g 10001 appuser && \
    useradd -u 10001 -g appuser -s /bin/bash -m appuser && \
    apt-get update && apt-get install -y --no-install-recommends curl && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Install Python dependencies
COPY backend/pyproject.toml .
RUN pip install --no-cache-dir .

# Copy application source
COPY backend/app /app/app

# Set ownership and runtime permissions
RUN mkdir -p /data && chown -R appuser:appuser /app /data

USER appuser

ENV PYTHONPATH=/app
ENV DATABASE_PATH=/data/fire_alarm.db
ENV DEVICE_CREDENTIALS_FILE=/data/device_credentials.json

HEALTHCHECK --interval=15s --timeout=5s --start-period=5s --retries=3 \
    CMD curl -fsS http://localhost:8000/healthz || exit 1

EXPOSE 8000

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
