# arangodb_ersp

<!-- TABLE OF CONTENTS -->
<details open="open">
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#summary">Summary</a>
    </li>
    <li><a href="#instructions">Instructions</a></li>
    <li><a href="#credits">Credits</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
  </ol>
</details>

<!-- ABOUT THE PROJECT -->

## Summary

The original codebase is sourced from ArangoDB.

We are looking into a way to incorporate queries related to graph embeddings. 

For example (note: this is not specific to AQL):
```
FOR researcher IN ‘academics’
	FILTER researcher.department == ‘computer science’

	FOR collaborator IN 1 . . 3 OUTBOUND researcher GRAPH ‘collaborationGraph’

	FILTER SIMILAR_TO(collaborator, ‘researchInterests’, researcher, 10)

	RETURN { researcherName: researcher.name, collaboratorName: collaborator.name }
```
<!-- Summary -->
## Instructions
### For members of the team:
1. Upon pull requesting, verify that the base fork is set to `wrcorcoran/arangodb_ersp`.
2. Comment delicately (in order to distinguish code from ArangoDB codebase).
3. Verify that your origin is set to proper branch using: ```git remote set-url origin https://github.com/wrcorcoran/arangodb_ersp.git```.

### For reproducibility:
> Soon to come.


<!-- Credits -->
## Credits
This work is purely research and built on top of [ArangoDB](https://github.com/arangodb/arangodb).

<!-- LICENSE -->
## License

Distributed under the Apache License 2.0. See `LICENSE` for more information.

<!-- CONTACT -->
## Contact

Will Corcoran - wcorcoran@ucsb.edu

Wyatt Hamabe - whamabe@ucsb.edu

Niyati Mummidivarapu - niyati@ucsb.edu

Danish Ebadulla - danish_ebadulla (at) umail (dot) ucsb (dot) edu
