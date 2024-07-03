# Ontology Knowledge

Uma ontologia descreve os recursos, de uma maneira estruturada, dentro do OneM2M.
Esta estrutura, OWL, deve ser/é interpretável computacionalmente.
Esta estrutura é constituida por "concepts", estes conceitos interligam-se, estas ligações chamam-se propriedades.
Um conjunto de conceitos bem estruturados, em OneM2M, é uma ontologia.

O objetivo de integrar uma ontologia no standard OneM2M é permitir a dispositivos fora do ambiente oneM2M conhecerem os recursos dentro deste.

Os conceitos não são os recursos do OneM2M, mas são descritivos desses recursos.

Em OWL (representação/standard de ontologia):
Concept -> Class
Relação -> Object Property
    - ClassA -> related_with -> ClassB
Informação do conceito / Atributos -> Data Property
    - Device -> hasManufacturer -> Yes
Informação adicional que não será considerada para pesquisa -> Annotation Property
    - Pode conter por exemplo uma versão, ou um comentário

As queries de pesquisa são escritas por SPARdfQueryLanguage

A descrição/escrita do standard OWL chama-se OWL-DL (Description Logic). O próprio standard refere o editor de ontologia "Protégé".
A única ontologia especificada pelo standard OneM2M é a [oneM2M Base Ontology](https://git.onem2m.org/MAS/BaseOntology/-/blob/master/base_ontology.owl).

SAREF is a standard of ontology that can extends the oneM2M Base Ontology. We will use SAREF standard to extends the oneM2M Base Ontology.
 - In SAREF, a WashingMachine class might exist with properties like hasProgram and hasTemperatureSetting.
 - This WashingMachine class can be mapped as a subclass of the Device class in the oneM2M Base Ontology.
 - The properties hasProgram and hasTemperatureSetting can be mapped to corresponding properties in the Base Ontology or added as new properties if they do not exist.

There will be a new resource, named SMD (SeManticDescriptor).
 - Can only have SUBs resourcers bellow.
 - Pode apenas descrever AEs e CNTs. ?????

