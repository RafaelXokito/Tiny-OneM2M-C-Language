class SUB:
    def __init__(self,
                 rn: str = None,
                 nu: list[str] | None = None,
                 enc: str = None,
                 et: str = None,
                 lbl: list[str] = None,
                 daci: list[str] = None) -> None:
        self.rn = rn
        self.nu = nu
        self.enc = enc
        self.et = et
        self.lbl = lbl
        self.daci = daci

    def to_json(self) -> dict[str, dict[str, str | list[str] | int]]:
        sub_dict = {}
        if self.rn is not None:
            sub_dict["rn"] = self.rn
        if self.nu is not None:
            sub_dict["nu"] = self.nu
        if self.enc is not None:
            sub_dict["enc"] = self.enc
        if self.et is not None:
            sub_dict["et"] = self.et
        if self.lbl is not None:
            sub_dict["lbl"] = self.lbl
        if self.daci is not None:
            sub_dict["daci"] = self.daci

        return {"m2m:sub": sub_dict}
