class CIN:
    def __init__(self,
                 cnf: str = None,
                 con: str = None,
                 rn: str = None,
                 et: str = None,
                 lbl: list[str] = None) -> None:
        self.cnf = cnf
        self.con = con
        self.rn = rn
        self.et = et
        self.lbl = lbl

    def to_json(self) -> dict[str, dict[str, str | list[str] | int]]:
        cin_dict = {}
        if self.cnf is not None:
            cin_dict["cnf"] = self.cnf
        if self.con is not None:
            cin_dict["con"] = self.con
        if self.rn is not None:
            cin_dict["rn"] = self.rn
        if self.et is not None:
            cin_dict["et"] = self.et
        if self.lbl is not None:
            cin_dict["lbl"] = self.lbl

        return {"m2m:cin": cin_dict}
