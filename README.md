# laser-monitor
Code for laser monitoring system

## Description

An open source remote monitoring system has been designed and built to address the needs of researchers to provide basic illuminated visual indication of laser operation for university research laboratories that are equipped with multiple types of high-powered lasers and have limited financial resources.

## Version Control Approach

Previously, we tracked different versions of our code by adding version numbers directly into filenames (e.g., monitor-v4.py). This approach made sense when working without formal version control, but it became cumbersome when collaborating, merging changes, and reviewing code.

To address this, we've adopted Git tagging for version tracking:

Single file per device type: Files are now simply named monitor.py and indicator.py.

Version tags: Each stable release is clearly tagged (e.g., v1.0) in GitHub.

Deployment tracking: Installations on hardware reference these Git version tags, ensuring full traceability.

This method simplifies collaboration, makes version comparisons clearer, and integrates seamlessly with standard GitHub practices.