import os
import sys
import tempfile
from pathlib import Path
import pytest
from fastapi.testclient import TestClient

BACKEND_DIR = Path(__file__).resolve().parent.parent
if str(BACKEND_DIR) not in sys.path:
    sys.path.insert(0, str(BACKEND_DIR))

from app.main import app
from app.db import init_db, get_connection
import app.config as config
import app.main as main_module

@pytest.fixture
def temp_db():
    with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as f:
        db_path = f.name

    init_db(db_path)
    old_db = config.DATABASE_PATH
    config.DATABASE_PATH = db_path
    main_module.DATABASE_PATH = db_path

    yield db_path

    config.DATABASE_PATH = old_db
    main_module.DATABASE_PATH = old_db
    if os.path.exists(db_path):
        try:
            os.remove(db_path)
        except Exception:
            pass

@pytest.fixture
def client(temp_db):
    return TestClient(app)

@pytest.fixture
def auth_headers():
    return {"Authorization": "Bearer secret-token-kitchen-01"}
