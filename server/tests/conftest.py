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
    from tests.factories import admin_headers, device_config

    with TestClient(app) as test_client:
        # The seeded device has no configuration until it reports one; give it
        # site coordinates so derived archive values are covered by default.
        response = test_client.put(
            "/api/v1/admin/devices/sim-001/config",
            json=device_config(),
            headers=admin_headers(),
        )
        assert response.status_code == 200, response.text
        yield test_client
