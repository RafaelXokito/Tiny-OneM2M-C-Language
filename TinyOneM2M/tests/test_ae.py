import os
import unittest
import uuid
from datetime import datetime, timedelta

import requests
from dotenv import load_dotenv

from tests.entities.AE import AE

load_dotenv()


class AETestCase(unittest.TestCase):
    base_url = os.getenv('BASE_URL')

    def test_create_ae(self):
        url = f"{self.base_url}/onem2m"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=2"
        }

        ae_entity = AE()
        payload = ae_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 200
        response_data = response.json()
        assert response_data["m2m:ae"]["api"] == ae_entity.api
        assert response_data["m2m:ae"]["rr"] == ae_entity.rr
        assert response_data["m2m:ae"]["lbl"] == []
        assert response_data["m2m:ae"]["poa"] == []
        assert response_data["m2m:ae"]["acpi"] == []

    def test_create_long_ae(self):
        url = f"{self.base_url}/onem2m"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=2"
        }

        rn = f"lightbulb_{uuid.uuid4().hex}"
        ae_entity = AE(
            rn=rn,
            lbl=[
                {
                    "XPTO": "XPTO123",
                    "XPTO1": "XPTO123",
                    "XPTO2": "XPTO123",
                    "XPTO3": {"XPTO123": "XPTO123"},
                    "lbl2": ["TAG", "TAG2", "TAG3"]
                },
                {
                    "XPTO": "XPTO123",
                    "XPTO1": "XPTO123",
                    "XPTO2": "XPTO123",
                    "XPTO4": [1, 2]
                },
                "TAG",
                {
                    "XPTO": {"XPTO": [1, 2]}
                },
                "TAG2",
                "TAG3",
                1,
                [1, 2]
            ],
            poa=["http://127.0.0.1:8080"],
            acpi=["/id-in/acpCreateACPs"]
        )
        payload = ae_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 200
        response_data = response.json()
        assert response_data["m2m:ae"]["api"] == ae_entity.api
        assert response_data["m2m:ae"]["rn"] == ae_entity.rn
        assert response_data["m2m:ae"]["rr"] == ae_entity.rr
        assert response_data["m2m:ae"]["lbl"] == ae_entity.lbl
        assert response_data["m2m:ae"]["poa"] == ae_entity.poa
        assert response_data["m2m:ae"]["acpi"] == ae_entity.acpi

    def test_create_ae_without_api(self):
        url = f"{self.base_url}/onem2m"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=2"
        }

        ae_entity = AE(api=None)
        payload = ae_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 400
        response_data = response.json()
        assert "api key not found" in response_data["message"]

    def test_create_ae_without_request_reachability(self):
        url = f"{self.base_url}/onem2m"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=2"
        }

        ae_entity = AE(rr=None)
        payload = ae_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 400
        response_data = response.json()
        assert "rr key not found" in response_data["message"]

    def test_retrieve_ae(self):
        create_url = f"{self.base_url}/onem2m"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=2"
        }

        ae_entity = AE()
        create_payload = ae_entity.to_json()
        create_response = requests.post(create_url, headers=headers, json=create_payload)
        assert create_response.status_code == 200
        created_ae_id = create_response.json()["m2m:ae"]["rn"]

        retrieve_url = f"{self.base_url}/onem2m/{created_ae_id}"
        retrieve_response = requests.get(retrieve_url, headers=headers)
        assert retrieve_response.status_code == 200
        response_data = retrieve_response.json()
        assert response_data["m2m:ae"]["rn"] == created_ae_id

    def test_retrieve_invalid_ae(self):
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=2"
        }

        rn = f"{uuid.uuid4().hex}"
        retrieve_url = f"{self.base_url}/onem2m/{rn}"
        retrieve_response = requests.get(retrieve_url, headers=headers)
        assert retrieve_response.status_code == 404
        response_data = retrieve_response.json()
        assert response_data["message"] == "Resource not found"

    def test_update_ae(self):
        create_url = f"{self.base_url}/onem2m"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=2"
        }

        ae_entity = AE()
        create_payload = ae_entity.to_json()
        create_response = requests.post(create_url, headers=headers, json=create_payload)
        assert create_response.status_code == 200
        created_ae_id = create_response.json()["m2m:ae"]["rn"]

        update_url = f"{self.base_url}/onem2m/{created_ae_id}"
        update_payload = {
            "m2m:ae": {
                "et": (datetime.now() + timedelta(days=60)).strftime('%Y%m%dT%H%M%S'),
                "poa": ["http://127.0.0.2:4314"]
            }
        }
        update_response = requests.put(update_url, headers=headers, json=update_payload)
        assert update_response.status_code == 200
        response_data = update_response.json()
        assert response_data["m2m:ae"]["et"] == update_payload["m2m:ae"]["et"]
        assert response_data["m2m:ae"]["poa"] == update_payload["m2m:ae"]["poa"]

    def test_update_invalid_ae(self):
        create_url = f"{self.base_url}/onem2m"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=2"
        }

        rn = f"{uuid.uuid4().hex}"

        update_url = f"{self.base_url}/onem2m/{rn}"
        update_payload = {
            "m2m:ae": {
                "poa": ["http://127.0.0.2:1234"]
            }
        }
        update_response = requests.put(update_url, headers=headers, json=update_payload)
        assert update_response.status_code == 404
        response_data = update_response.json()
        assert response_data["message"] == "Resource not found"

    def test_delete_ae(self):
        create_url = f"{self.base_url}/onem2m"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=2"
        }

        ae_entity = AE()
        create_payload = ae_entity.to_json()
        create_response = requests.post(create_url, headers=headers, json=create_payload)
        assert create_response.status_code == 200
        created_ae_id = create_response.json()["m2m:ae"]["rn"]

        delete_url = f"{self.base_url}/onem2m/{created_ae_id}"
        delete_response = requests.delete(delete_url, headers=headers)
        assert delete_response.status_code == 200

        # Verify deletion
        verify_response = requests.get(delete_url, headers=headers)
        assert verify_response.status_code == 404


if __name__ == '__main__':
    unittest.main()
