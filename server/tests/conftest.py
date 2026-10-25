import pytest
from fastapi.testclient import TestClient

from akashvani_server.config import Settings
from akashvani_server.main import create_app


@pytest.fixture()
def settings(tmp_path):
    return Settings(
        environment="test",
        database_url=f"sqlite:///{tmp_path / 'test.db'}",
        data_dir=tmp_path,
        firmware_dir=tmp_path / "firmware",
        device_seeds="sim-001:dev-sim-token:Simulator",
        admin_token="test-admin-token",
        log_level="WARNING",
    )


@pytest.fixture()
def app(settings):
    return create_app(settings)


@pytest.fixture()
def client(app):
    with TestClient(app) as test_client:
        yield test_client
