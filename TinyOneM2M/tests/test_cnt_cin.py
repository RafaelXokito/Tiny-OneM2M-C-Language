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


class CNTvCINTestCase(unittest.TestCase):
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

        cnt_entity = CNT(mni=3, mbs=1024)
        cnt_payload = cnt_entity.to_json()
        cnt_response = requests.post(cnt_url, headers=cnt_headers, json=cnt_payload)
        assert cnt_response.status_code == 200
        cnt_response_data = cnt_response.json()
        cls.cnt_rn = cnt_response_data["m2m:cnt"]["rn"]

    def test_create_cin_and_update_cnt_attributes(self):
        url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=4"
        }

        cin_entity = CIN(cnf="application/json", con="Some content")
        payload = cin_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 200

        # Retrieve the container to check updated attributes
        cnt_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        cnt_response = requests.get(cnt_url, headers=headers)
        assert cnt_response.status_code == 200
        cnt_data = cnt_response.json()
        assert cnt_data["m2m:cnt"]["cni"] == 1
        assert cnt_data["m2m:cnt"]["st"] == 1
        assert cnt_data["m2m:cnt"]["cbs"] >= len(cin_entity.con)

    def test_max_nr_of_instances(self):
        url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=4"
        }

        for i in range(3):
            cin_entity = CIN(cnf="application/json", con=f"Content {i}")
            payload = cin_entity.to_json()
            response = requests.post(url, headers=headers, json=payload)
            assert response.status_code == 200

        # Adding one more CIN should respect the max number of instances
        cin_entity = CIN(cnf="application/json", con="Exceed content")
        payload = cin_entity.to_json()
        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 200

        # Retrieve the container to check if the oldest instance was removed
        cnt_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        cnt_response = requests.get(cnt_url, headers=headers)
        assert cnt_response.status_code == 200
        cnt_data = cnt_response.json()
        assert cnt_data["m2m:cnt"]["cni"] == 3  # Should be 3 due to mni limit
        assert cnt_data["m2m:cnt"]["st"] == 4  # State tag should increase with each addition

    def test_max_byte_size(self):
        url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=4"
        }

        large_content = "a" * 1024
        cin_entity = CIN(cnf="application/json", con=large_content)
        payload = cin_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 200

        # Adding a CIN that exceeds max byte size
        large_content = "b" * 1025
        cin_entity = CIN(cnf="application/json", con=large_content)
        payload = cin_entity.to_json()

        response = requests.post(url, headers=headers, json=payload)
        assert response.status_code == 400
        response_data = response.json()
        assert "max byte size exceeded" in response_data["message"]

    def test_cin_affects_cni_cbs_st(self):
        url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        headers = {
            "X-M2M-Origin": "admin:admin",
            "Content-Type": "application/json;ty=4"
        }

        initial_cin_entity = CIN(cnf="application/json", con="Initial content")
        initial_payload = initial_cin_entity.to_json()
        initial_response = requests.post(url, headers=headers, json=initial_payload)
        assert initial_response.status_code == 200

        # Retrieve the container after initial CIN creation
        cnt_url = f"{self.base_url}/onem2m/{self.ae_rn}/{self.cnt_rn}"
        cnt_response = requests.get(cnt_url, headers=headers)
        assert cnt_response.status_code == 200
        initial_cnt_data = cnt_response.json()
        initial_cni = initial_cnt_data["m2m:cnt"]["cni"]
        initial_cbs = initial_cnt_data["m2m:cnt"]["cbs"]
        initial_st = initial_cnt_data["m2m:cnt"]["st"]

        # Add another CIN
        new_cin_entity = CIN(cnf="application/json", con="New content")
        new_payload = new_cin_entity.to_json()
        new_response = requests.post(url, headers=headers, json=new_payload)
        assert new_response.status_code == 200

        # Retrieve the container again to check the updated attributes
        cnt_response = requests.get(cnt_url, headers=headers)
        assert cnt_response.status_code == 200
        new_cnt_data = cnt_response.json()
        assert new_cnt_data["m2m:cnt"]["cni"] == initial_cni + 1
        assert new_cnt_data["m2m:cnt"]["cbs"] == initial_cbs + len(new_cin_entity.con)
        assert new_cnt_data["m2m:cnt"]["st"] == initial_st


if __name__ == '__main__':
    unittest.main()
