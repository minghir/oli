for f in ./tmp/dbfs/*.DBF; do
    mv "$f" "${f%.DBF}.dbf"
done
