class AE:
    def __init__(self,
                 api: str | None = "placeholder",
                 rr: bool | None = True,
                 rn: str = None,
                 et: str = None,
                 lbl: list[str] = None,
                 poa: list[str] = None,
                 acpi: list[str] = None) -> None:
        self.api = api
        self.rr = rr
        self.rn = rn
        self.et = et
        self.lbl = lbl
        self.poa = poa
        self.acpi = acpi

    def to_json(self) -> dict[str, dict[str, str | list[str]]]:
        ae_dict = {}
        if self.api is not None:
            ae_dict["api"] = self.api
        if self.rr is not None:
            ae_dict["rr"] = str(self.rr).lower()
        if self.rn is not None:
            ae_dict["rn"] = self.rn
        if self.et is not None:
            ae_dict["et"] = self.et
        if self.lbl is not None:
            ae_dict["lbl"] = self.lbl
        if self.poa is not None:
            ae_dict["poa"] = self.poa
        if self.acpi is not None:
            ae_dict["acpi"] = self.acpi

        return {"m2m:ae": ae_dict}
