{
  "targets": [
    {
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ],
      "target_name": "isotp",
      "sources": [ "src/isotp.cpp" ]
    }
  ]
}