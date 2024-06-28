import os
import unittest
import uuid
from datetime import datetime, timedelta

import requests
from dotenv import load_dotenv

from tests.entities.AE import AE
from tests.entities.CNT import CNT
from tests.entities.SUB import SUB

load_dotenv()


# TODO - Take into account that we could create a subscription inside any resource
# TODO - A subscription is more than a resource, we need to test if the subscription is working with mqtt.

class SUBTestCase(unittest.TestCase):
    base_url = os.getenv('BASE_URL')

    @classmethod
    def setUpClass(cls):
        url = f"{cls.base_url}/onem2m"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=2"
        }

        ae_entity = AE()
        payload = ae_entity.to_json()
        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 200
        response_data = response.json()
        cls.ae_rn = response_data["m2m:ae"]["rn"]

        cnt_url = f"{cls.base_url}/onem2m/{cls.ae_rn}"
        headers["Content-Type"] = "application/json;ty=3"
        cnt_entity = CNT()
        cnt_payload = cnt_entity.to_json()
        cnt_response = requests.post(cnt_url, headers=headers, json=cnt_payload)
        assert cnt_response.status_code == 200
        cnt_response_data = cnt_response.json()
        cls.cnt_rn = cnt_response_data["m2m:cnt"]["rn"]

    def test_create_sub(self):
        url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=23"
        }

        sub_entity = SUB(nu=["http://127.0.0.1:8080"])
        payload = sub_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 200
        response_data = response.json()
        assert response_data["m2m:sub"]["nu"] == sub_entity.nu
        assert response_data["m2m:sub"]["enc"] == "POST, PUT, GET, DELETE"

    def test_create_long_sub(self):
        url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=23"
        }

        rn = f"SUB_{uuid.uuid4().hex}"
        sub_entity = SUB(
            rn=rn,
            nu=["http://127.0.0.1:8080", "http://127.0.0.1:8081"],
            enc="POST",
            et=(datetime.now() + timedelta(days=60)).strftime('%Y%m%dT%H%M%S'),
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
            daci=["subscription_uri"]
        )
        payload = sub_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 200
        response_data = response.json()
        assert response_data["m2m:sub"]["rn"] == sub_entity.rn
        assert response_data["m2m:sub"]["nu"] == sub_entity.nu
        assert response_data["m2m:sub"]["enc"] == sub_entity.enc
        assert response_data["m2m:sub"]["et"] == sub_entity.et
        assert response_data["m2m:sub"]["lbl"] == sub_entity.lbl
        assert response_data["m2m:sub"]["daci"] == sub_entity.daci

    def test_create_sub_without_notification_uri(self):
        url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=23"
        }

        sub_entity = SUB(nu=None)
        payload = sub_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 400
        response_data = response.json()
        assert "nu key not found" in response_data["message"]

    def test_retrieve_sub(self):
        create_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=23"
        }

        sub_entity = SUB(nu=["http://127.0.0.1:8080"],
                         enc="POST",
                         et=(datetime.now() + timedelta(days=60)).strftime('%Y%m%dT%H%M%S'))
        create_payload = sub_entity.to_json()
        create_response = requests.post(create_url, headers=headers, json=create_payload)
        assert create_response.status_code == 200
        created_sub_id = create_response.json()["m2m:sub"]["rn"]

        retrieve_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}/{created_sub_id}"
        retrieve_response = requests.get(retrieve_url, headers=headers)
        assert retrieve_response.status_code == 200
        response_data = retrieve_response.json()
        assert response_data["m2m:sub"]["rn"] == created_sub_id
        assert response_data["m2m:sub"]["et"] == sub_entity.et
        assert response_data["m2m:sub"]["enc"] == sub_entity.enc

    def test_retrieve_invalid_sub(self):
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=23"
        }

        rn = f"{uuid.uuid4().hex}"
        retrieve_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}/{rn}"
        retrieve_response = requests.get(retrieve_url, headers=headers)
        assert retrieve_response.status_code == 404
        response_data = retrieve_response.json()
        assert response_data["message"] == "Resource not found"

    def test_update_sub(self):
        create_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=23"
        }

        sub_entity = SUB(nu=["http://127.0.0.1:8080"], enc="POST")
        create_payload = sub_entity.to_json()
        create_response = requests.post(create_url, headers=headers, json=create_payload)
        assert create_response.status_code == 200
        created_sub_id = create_response.json()["m2m:sub"]["rn"]

        update_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}/{created_sub_id}"
        sub_entity.nu = ["http://127.0.0.2:4314"]
        sub_entity.enc = "POST, PUT"

        update_response = requests.put(update_url, headers=headers, json=sub_entity.to_json())
        assert update_response.status_code == 200
        response_data = update_response.json()
        assert response_data["m2m:sub"]["nu"] == sub_entity.nu
        assert response_data["m2m:sub"]["enc"] == sub_entity.enc

    def test_update_invalid_sub(self):
        create_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=23"
        }

        rn = f"{uuid.uuid4().hex}"

        update_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}/{rn}"
        update_payload = {
            "m2m:sub": {
                "nu": ["http://127.0.0.2:1234"]
            }
        }
        update_response = requests.put(update_url, headers=headers, json=update_payload)
        assert update_response.status_code == 404
        response_data = update_response.json()
        assert response_data["message"] == "Resource not found"

    def test_delete_sub(self):
        create_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=23"
        }

        sub_entity = SUB(nu=["http://127.0.0.1:8080"])
        create_payload = sub_entity.to_json()
        create_response = requests.post(create_url, headers=headers, json=create_payload)
        assert create_response.status_code == 200
        created_sub_id = create_response.json()["m2m:sub"]["rn"]

        delete_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}/{created_sub_id}"
        delete_response = requests.delete(delete_url, headers=headers)
        assert delete_response.status_code == 200

        # Verify deletion
        verify_response = requests.get(delete_url, headers=headers)
        assert verify_response.status_code == 404


if __name__ == '__main__':
    unittest.main()
