import {useState} from 'react';
import styles from  './Spinner.module.css'

export default function Spinner({val, min, max, step, onChange}) {
    const [value, setValue] = useState(val);

    function handleClickPlus() {
        if (value < max) {
            const newValue = value + step;
            setValue(newValue);
            if (onChange) onChange(newValue);
        }
    }

    function handleClickMinus() {
        if (value > min) {
            const newValue = value - step;
            setValue(newValue);
            if (onChange) onChange(newValue);
        }
    }

    return (
        <div className={styles.spinner}>
            <div onClick={handleClickMinus}>
                <span>-</span>
            </div>
            <div>
                <span>{value}</span>
            </div>
            <div onClick={handleClickPlus}>
                <span>+</span>
            </div>
        </div>
    )
}