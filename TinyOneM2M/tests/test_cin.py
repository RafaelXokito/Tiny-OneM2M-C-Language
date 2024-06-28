import os
import unittest
import uuid
from datetime import datetime, timedelta

import requests
from dotenv import load_dotenv

from tests.entities.AE import AE
from tests.entities.CNT import CNT
from tests.entities.CIN import CIN

load_dotenv()


class CINTestCase(unittest.TestCase):
    base_url = os.getenv('BASE_URL')

    @classmethod
    def setUpClass(cls):
        ae_url = f"{cls.base_url}/onem2m"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=2"
        }

        ae_entity = AE()
        ae_payload = ae_entity.to_json()
        ae_response = requests.post(ae_url, headers=headers, json=ae_payload)
        assert ae_response.status_code == 200
        ae_response_data = ae_response.json()
        cls.ae_rn = ae_response_data["m2m:ae"]["rn"]

        cnt_url = f"{cls.base_url}/onem2m/{cls.ae_rn}"
        cnt_headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=3"
        }

        cnt_entity = CNT()
        cnt_payload = cnt_entity.to_json()
        cnt_response = requests.post(cnt_url, headers=cnt_headers, json=cnt_payload)
        assert cnt_response.status_code == 200
        cnt_response_data = cnt_response.json()
        cls.cnt_rn = cnt_response_data["m2m:cnt"]["rn"]

    def test_create_cin(self):
        url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=4"
        }

        cin_entity = CIN(cnf="application/json", con="Some content")
        payload = cin_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 200
        response_data = response.json()
        assert response_data["m2m:cin"]["cnf"] == cin_entity.cnf
        assert response_data["m2m:cin"]["con"] == cin_entity.con

    def test_create_cin_with_all_fields(self):
        url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=4"
        }

        rn = f"CIN_{uuid.uuid4().hex}"
        cin_entity = CIN(
            cnf="application/json",
            con="Some detailed content",
            rn=rn,
            et=(datetime.now() + timedelta(days=365)).strftime('%Y%m%dT%H%M%S'),
            lbl=["tag1", "tag2"]
        )
        payload = cin_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 200
        response_data = response.json()
        assert response_data["m2m:cin"]["cnf"] == cin_entity.cnf
        assert response_data["m2m:cin"]["con"] == cin_entity.con
        assert response_data["m2m:cin"]["rn"] == cin_entity.rn
        assert response_data["m2m:cin"]["et"] == cin_entity.et
        assert response_data["m2m:cin"]["lbl"] == cin_entity.lbl

    def test_retrieve_cin(self):
        create_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=4"
        }

        cin_entity = CIN(cnf="application/json", con="Some content")
        create_payload = cin_entity.to_json()
        create_response = requests.post(create_url, headers=headers, json=create_payload)
        assert create_response.status_code == 200
        created_cin_id = create_response.json()["m2m:cin"]["rn"]

        retrieve_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}/{created_cin_id}"
        retrieve_response = requests.get(retrieve_url, headers=headers)
        assert retrieve_response.status_code == 200
        response_data = retrieve_response.json()
        assert response_data["m2m:cin"]["rn"] == created_cin_id

    def test_retrieve_invalid_cin(self):
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=4"
        }

        rn = f"{uuid.uuid4().hex}"
        retrieve_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}/{rn}"
        retrieve_response = requests.get(retrieve_url, headers=headers)
        assert retrieve_response.status_code == 404
        response_data = retrieve_response.json()
        assert response_data["message"] == "Resource not found"

    def test_update_cin(self):
        # CIN resource is not updatable
        create_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=4"
        }

        cin_entity = CIN(cnf="application/json", con="Some content")
        create_payload = cin_entity.to_json()
        create_response = requests.post(create_url, headers=headers, json=create_payload)
        assert create_response.status_code == 200
        created_cin_id = create_response.json()["m2m:cin"]["rn"]

        update_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}/{created_cin_id}"
        update_payload = {
            "m2m:cin": {
                "con": "Updated content",
                "lbl": ["newTag"]
            }
        }
        update_response = requests.put(update_url, headers=headers, json=update_payload)
        assert update_response.status_code == 400
        response_data = update_response.json()
        assert response_data["message"] == "Invalid resource"

    def test_update_invalid_cin(self):
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=4"
        }

        rn = f"{uuid.uuid4().hex}"

        update_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}/{rn}"
        update_payload = {
            "m2m:cin": {
                "con": "Invalid update content"
            }
        }
        update_response = requests.put(update_url, headers=headers, json=update_payload)
        assert update_response.status_code == 404
        response_data = update_response.json()
        assert response_data["message"] == "Resource not found"

    def test_delete_cin(self):
        create_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=4"
        }

        cin_entity = CIN(cnf="application/json", con="Some content")
        create_payload = cin_entity.to_json()
        create_response = requests.post(create_url, headers=headers, json=create_payload)
        assert create_response.status_code == 200
        created_cin_id = create_response.json()["m2m:cin"]["rn"]

        delete_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}/{created_cin_id}"
        delete_response = requests.delete(delete_url, headers=headers)
        assert delete_response.status_code == 200

        # Verify deletion
        verify_response = requests.get(delete_url, headers=headers)
        assert verify_response.status_code == 404


if __name__ == '__main__':
    unittest.main()
