class CNT:
    def __init__(self,
                 rn: str = None,
                 et: str = None,
                 lbl: list[str] = None,
                 mni: int = None,
                 mbs: int = None,
                 mia: int = None,
                 acpi: list[str] = None,
                 aa: list[str] = None) -> None:
        self.rn = rn
        self.et = et
        self.lbl = lbl
        self.mni = mni
        self.mbs = mbs
        self.mia = mia
        self.acpi = acpi
        self.aa = aa

    def to_json(self) -> dict[str, dict[str, str | list[str] | int]]:
        container_dict = {}
        if self.rn is not None:
            container_dict["rn"] = self.rn
        if self.et is not None:
            container_dict["et"] = self.et
        if self.lbl is not None:
            container_dict["lbl"] = self.lbl
        if self.mni is not None:
            container_dict["mni"] = self.mni
        if self.mbs is not None:
            container_dict["mbs"] = self.mbs
        if self.mia is not None:
            container_dict["mia"] = self.mia
        if self.acpi is not None:
            container_dict["acpi"] = self.acpi
        if self.aa is not None:
            container_dict["aa"] = self.aa

        return {"m2m:cnt": container_dict}
