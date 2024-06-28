import os
import unittest
import uuid
from datetime import datetime, timedelta

import requests
from dotenv import load_dotenv

from tests.entities.AE import AE
from tests.entities.CNT import CNT

load_dotenv()


class CNTTestCase(unittest.TestCase):
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

    def test_create_cnt(self):
        url = f"{self.base_url}/onem2m/{self.ae_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=3"
        }

        cnt_entity = CNT()
        payload = cnt_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 200

    def test_create_cnt_with_empty_label_list(self):
        url = f"{self.base_url}/onem2m/{self.ae_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=3"
        }

        cnt_entity = CNT(lbl=[])
        payload = cnt_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 200
        response_data = response.json()
        assert response_data["m2m:cnt"]["lbl"] == cnt_entity.lbl

    def test_create_cnt_with_special_characters_in_rn(self):
        url = f"{self.base_url}/onem2m/{self.ae_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=3"
        }

        special_rn = "CNT_!@#$%^&*()_+|"
        cnt_entity = CNT(rn=special_rn)
        payload = cnt_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 200
        response_data = response.json()
        rn_without_special_characters = "CNT__"
        assert response_data["m2m:cnt"]["rn"] == rn_without_special_characters

    def test_create_cnt_with_past_expiration_time(self):
        url = f"{self.base_url}/onem2m/{self.ae_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=3"
        }

        past_et = (datetime.now() - timedelta(days=1)).strftime('%Y%m%dT%H%M%S')
        cnt_entity = CNT(et=past_et)
        payload = cnt_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 400
        response_data = response.json()
        assert "Expiration time is in the past" in response_data["message"]

    def test_create_cnt_with_specific_values(self):
        url = f"{self.base_url}/onem2m/{self.ae_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=3"
        }

        rn = f"CNT_{uuid.uuid4().hex}"
        cnt_entity = CNT(
            rn=rn,
            lbl=["tag1", "tag2"],
            mni=10,
            mbs=1024,
            mia=3600,
            acpi=["/id-in/acpCreateACPs"],
            aa=["activity1", "activity2"]
        )
        payload = cnt_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 200
        response_data = response.json()
        assert response_data["m2m:cnt"]["rn"] == cnt_entity.rn
        assert response_data["m2m:cnt"]["lbl"] == cnt_entity.lbl
        assert response_data["m2m:cnt"]["mni"] == cnt_entity.mni
        assert response_data["m2m:cnt"]["mbs"] == cnt_entity.mbs
        assert response_data["m2m:cnt"]["acpi"] == cnt_entity.acpi

    def test_retrieve_cnt(self):
        create_url = f"{self.base_url}/onem2m/{self.ae_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=3"
        }

        cnt_entity = CNT()
        create_payload = cnt_entity.to_json()
        create_response = requests.post(create_url, headers=headers, json=create_payload)
        assert create_response.status_code == 200
        created_cnt_id = create_response.json()["m2m:cnt"]["rn"]

        retrieve_url = f"{self.base_url}/onem2m/{self.ae_rn}/{created_cnt_id}"
        retrieve_response = requests.get(retrieve_url, headers=headers)
        assert retrieve_response.status_code == 200
        response_data = retrieve_response.json()
        assert response_data["m2m:cnt"]["rn"] == created_cnt_id

    def test_retrieve_invalid_cnt(self):
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=3"
        }

        rn = f"{uuid.uuid4().hex}"
        retrieve_url = f"{self.base_url}/onem2m/{self.ae_rn}/{rn}"
        retrieve_response = requests.get(retrieve_url, headers=headers)
        assert retrieve_response.status_code == 404
        response_data = retrieve_response.json()
        assert response_data["message"] == "Resource not found"

    def test_update_cnt(self):
        create_url = f"{self.base_url}/onem2m/{self.ae_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=3"
        }

        cnt_entity = CNT()
        create_payload = cnt_entity.to_json()
        create_response = requests.post(create_url, headers=headers, json=create_payload)
        assert create_response.status_code == 200
        created_cnt_id = create_response.json()["m2m:cnt"]["rn"]

        update_url = f"{self.base_url}/onem2m/{self.ae_rn}/{created_cnt_id}"
        update_payload = {
            "m2m:cnt": {
                "et": (datetime.now() + timedelta(days=60)).strftime('%Y%m%dT%H%M%S'),
                "lbl": ["newTag"]
            }
        }
        update_response = requests.put(update_url, headers=headers, json=update_payload)
        assert update_response.status_code == 200
        response_data = update_response.json()
        assert response_data["m2m:cnt"]["et"] == update_payload["m2m:cnt"]["et"]
        assert response_data["m2m:cnt"]["lbl"] == update_payload["m2m:cnt"]["lbl"]

    def test_update_invalid_cnt(self):
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=3"
        }

        rn = f"{uuid.uuid4().hex}"

        update_url = f"{self.base_url}/onem2m/{self.ae_rn}/{rn}"
        update_payload = {
            "m2m:cnt": {
                "lbl": ["invalidUpdate"]
            }
        }
        update_response = requests.put(update_url, headers=headers, json=update_payload)
        assert update_response.status_code == 404
        response_data = update_response.json()
        assert response_data["message"] == "Resource not found"

    def test_delete_cnt(self):
        create_url = f"{self.base_url}/onem2m/{self.ae_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=3"
        }

        cnt_entity = CNT()
        create_payload = cnt_entity.to_json()
        create_response = requests.post(create_url, headers=headers, json=create_payload)
        assert create_response.status_code == 200
        created_cnt_id = create_response.json()["m2m:cnt"]["rn"]

        delete_url = f"{self.base_url}/onem2m/{self.ae_rn}/{created_cnt_id}"
        delete_response = requests.delete(delete_url, headers=headers)
        assert delete_response.status_code == 200

        # Verify deletion
        verify_response = requests.get(delete_url, headers=headers)
        assert verify_response.status_code == 404


if __name__ == '__main__':
    unittest.main()
