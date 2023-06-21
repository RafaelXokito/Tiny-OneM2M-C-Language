import requests

url = "http://10.79.12.208:8000/onem2m/lightbulb/state"

payload = "{\r\n    \"m2m:sub\": {\r\n        \"nu\":   [\"mqtt://10.79.12.253:1883\"],\r\n        \"enc\":  \"POST, PUT, GET, DELETE\"\r\n    }\r\n}\r\n"
headers = {
  'X-M2M-Origin': 'admin:admin',
  'Content-Type': 'application/json;ty=23'
}

response = requests.request("POST", url, headers=headers, data=payload)

print(response.text)
