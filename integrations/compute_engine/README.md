## Google Cloud Compute Engine

We have setup a Virtual Machine on
[Google Cloud](https://cloud.google.com/products/compute) to generate both the
[Matter SDK coverage report](https://matter-build-automation.ue.r.appspot.com)
and the
[Matter SDK Conformance report](https://matter-build-automation.ue.r.appspot.com/conformance_report.html).

### The Matter SDK Virtual Machine and the "startup-script.sh"

We created a VM named `matter-build-coverage`. The machine configuration is
located
[here](https://pantheon.corp.google.com/compute/instancesDetail/zones/us-central1-a/instances/matter-build-coverage?inv=1&invt=AbnAfg&project=matter-build-automation).
Reach out to Google team members if you need to make changes to this VM.

This virtual machine is scheduled to run daily, starting at 11:45PM and stopping
at 2am. During boot, the machine runs the `startup-script.sh`.

The `startup-script.sh` script contains commands to checkout the SDK repository
and run the master reporting script `integrations/compute_engine/automated_reports.sh` with the
`--all` and `--deploy` flags.

The resulting HTML files are published via an App Engine service and available here:
- [Coverage Report](https://matter-build-automation.ue.r.appspot.com/)
- [Conformance Report](https://matter-build-automation.ue.r.appspot.com/conformance_report.html)

### Local Testing

To test the reports locally without starting the GCE instance or deploying to App Engine, you can use the `automated_reports.sh` script directly.

**Note:** The script must be run from the root of the repository.

Example: Run only the conformance report and view it locally:
```bash
./integrations/compute_engine/automated_reports.sh --conformance --serve
```

This will generate the conformance report (and a dummy page for the coverage report) and start a local web server at `http://localhost:8000`.

### Making Changes to "startup-script.sh"

If you make changes to `startup-script.sh` itself (e.g., changing the repo URL or branch), make sure you go to the
[VM configuration](https://pantheon.corp.google.com/compute/instancesDetail/zones/us-central1-a/instances/matter-build-coverage?inv=1&invt=AbnAfg&project=matter-build-automation),
click `edit` and update the startup script in the `Automation` text box, to
reflect your changes.

However, if you only need to change the reporting logic, you can simply modify `integrations/compute_engine/automated_reports.sh` in the repository.
